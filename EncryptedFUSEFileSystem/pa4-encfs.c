/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Minor modifications and note by Andy Sayler (2012) <www.andysayler.com>

  Source: fuse-2.8.7.tar.gz examples directory
  http://sourceforge.net/projects/fuse/files/fuse-2.X/

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags` fusexmp.c -o fusexmp `pkg-config fuse --libs`

  Note: This implementation is largely stateless and does not maintain
        open file handels between open and release calls (fi->fh).
        Instead, files are opened and closed as necessary inside read(), write(),
        etc calls. As such, the functions that rely on maintaining file handles are
        not implmented (fgetattr(), etc). Those seeking a more efficient and
        more complete implementation may wish to add fi->fh support to minimize
        open() and close() calls and support fh dependent functions.

*/

#define FUSE_USE_VERSION 28
#define HAVE_SETXATTR

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include "aes-crypt.h"
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

//----Thank you, https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/html/private.html----
// maintain fs state in here
#include <limits.h>
#include <stdio.h>
typedef struct {
    char *rootdir;
    char *key;
} fs_state;
#define FS_DATA ((fs_state *) fuse_get_context()->private_data)

FILE *open_memstream(char **ptr, size_t *sizeloc);

static void fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, FS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
}

//-----------------------------------------------------------------------------------

static int pa4_encfs_getattr(const char *path, struct stat *stbuf)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//lstat: get file status
	res = lstat(fullPath, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_access(const char *path, int mask)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//access: check user's permissions for file
	res = access(fullPath, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_readlink(const char *path, char *buf, size_t size)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//readlink: print the value of a symbolic link or canonical file name
	res = readlink(fullPath, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int pa4_encfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//opendir: open a directory
	dp = opendir(fullPath);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int pa4_encfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(fullPath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(fullPath, mode);
	else
		res = mknod(fullPath, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_mkdir(const char *path, mode_t mode)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//mkdir: make a directory
	res = mkdir(fullPath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_unlink(const char *path)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//unlink: remove the specified file.
	res = unlink(fullPath);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_rmdir(const char *path)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//rmdir: remove a directory
	res = rmdir(fullPath);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_symlink(const char *from, const char *to)
{
	int res;

	//symlink: create a symbolic link named "to" which has the string "from"
	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_rename(const char *from, const char *to)
{
	int res;

	//rename: rename file
	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_chmod(const char *path, mode_t mode)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//chmod: change permissions on a file
	res = chmod(fullPath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//lchown: change the owner and group of a file
	res = lchown(fullPath, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_truncate(const char *path, off_t size)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//truncate: shrink or extend the size of a file
	res = truncate(fullPath, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	//utimes: change file last access and modification times
	res = utimes(fullPath, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	char fullPath[PATH_MAX]; 
	fullpath(fullPath, path);

	//open: open a file
	res = open(fullPath, fi->flags);
	if (res == -1)
		return -errno;

	close(res);

	return 0;
}

static int pa4_encfs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int res;

	//set up file and mirrored file
	FILE* file;
	FILE* mirrorFile;

	//buffer the value and length to be written into for mirroring 
	char xval[5];
	(void) fi;
	char* val;
	size_t valLength;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	//fopen: open file with read permissions
	file = fopen(fullPath, "r"); 
	if (file == NULL) {
		return -errno;
	}

	//set file buffer and length for mirror file directory
	mirrorFile = open_memstream(&val, &valLength);
	if(mirrorFile == NULL) {
		return -errno;
	}

	if(getxattr(fullPath, "user.encrypted", xval, 5) != -1) {	
		do_crypt(file, mirrorFile, 0, FS_DATA -> key); //if encrypted decrypt and copy 
	} else {
		do_crypt(file, mirrorFile, -1, FS_DATA -> key); //if not encrypted pass through case and copies
	}

	fflush(mirrorFile); //wait for the output file
	fseek(mirrorFile, offset, SEEK_SET); //determine load location in the mirror
 
	res = fread(buf, 1, size, mirrorFile);
	if (res == -1) {
		res = -errno;
	}

	fclose(file);
	fclose(mirrorFile);

	return res;
}

static int pa4_encfs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int res;

	//set up file and mirrored file
	FILE* file;
	FILE* mirrorFile;

	//buffer the value and length to be written into for mirroring
	char xval[5]; //for xtended attribute "encrypted"
	char* val;
	size_t valLength;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	(void) fi;

	file = fopen(fullPath, "r"); 
	if (file == NULL) {
		return -errno;
	}
	
	//set file buffer and length for mirror file directory
	mirrorFile = open_memstream(&val, &valLength);
	if (mirrorFile == NULL) {
		return -errno;
	}

	if (getxattr(fullPath, "user.encrypted", xval, 5) != -1) {
		do_crypt(file, mirrorFile, 0, FS_DATA -> key);
	}

	fseek(mirrorFile, offset, SEEK_SET);
	
	res = fwrite(buf, 1, size, mirrorFile);
	if (res == -1) {
		res = -errno;
	}
	
	fflush(mirrorFile);

	fclose(file);
	file = fopen(fullPath, "w");

	fseek(mirrorFile, 0, SEEK_SET);

	//write to file (writing always encrypts)
	do_crypt(mirrorFile, file, 1, FS_DATA -> key);

	setxattr(fullPath, "user.encrypted", "true", 4, 0);

	fclose(file);
	fclose(mirrorFile);

	return res;
}

static int pa4_encfs_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	//statvfs: get file system stats
	res = statvfs(fullPath, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int pa4_encfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
	(void) fi;
	(void) mode;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	int res;
	
	FILE* file;
	FILE* mirrorFile; 

	char* val;
	size_t valLength;

	file = fopen(fullPath, "w");
	if(file == NULL)
		return -errno;
	
	mirrorFile = open_memstream(&val, &valLength);
	if(mirrorFile == NULL) {
		return -errno;
	}
	
	do_crypt(mirrorFile, file, 1, FS_DATA -> key);
	
	res = setxattr(fullPath, "user.encrypted", "true", 4, 0);
	if(res) {
		return -errno;
	}

	fclose(file); 
	fclose(mirrorFile);

	return 0;
}


static int pa4_encfs_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int pa4_encfs_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
static int pa4_encfs_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	res = lsetxattr(fullPath, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int pa4_encfs_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	res = lgetxattr(fullPath, name, value, size);
	if (res == -1)
		return -errno;

	return res;
}

static int pa4_encfs_listxattr(const char *path, char *list, size_t size)
{
	int res;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	res = llistxattr(fullPath, list, size);
	if (res == -1)
		return -errno;

	return res;
}

static int pa4_encfs_removexattr(const char *path, const char *name)
{
	int res;

	char fullPath[PATH_MAX];
	fullpath(fullPath, path);

	res = lremovexattr(fullPath, name);
	if (res == -1)
		return -errno;
	
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations pa4_encfs_oper = {
	.getattr	= pa4_encfs_getattr,
	.access		= pa4_encfs_access,
	.readlink	= pa4_encfs_readlink,
	.readdir	= pa4_encfs_readdir,
	.mknod		= pa4_encfs_mknod,
	.mkdir		= pa4_encfs_mkdir,
	.symlink	= pa4_encfs_symlink,
	.unlink		= pa4_encfs_unlink,
	.rmdir		= pa4_encfs_rmdir,
	.rename		= pa4_encfs_rename,
	.link		= pa4_encfs_link,
	.chmod		= pa4_encfs_chmod,
	.chown		= pa4_encfs_chown,
	.truncate	= pa4_encfs_truncate,
	.utimens	= pa4_encfs_utimens,
	.open		= pa4_encfs_open,
	.read		= pa4_encfs_read,
	.write		= pa4_encfs_write,
	.statfs		= pa4_encfs_statfs,
	.create     = pa4_encfs_create,
	.release	= pa4_encfs_release,
	.fsync		= pa4_encfs_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= pa4_encfs_setxattr,
	.getxattr	= pa4_encfs_getxattr,
	.listxattr	= pa4_encfs_listxattr,
	.removexattr= pa4_encfs_removexattr,
#endif
};

int main(int argc, char *argv[]) 
{
	//Thanks, http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/html/init.html

	fs_state *fsState;
	umask(0);

	//Usage: ./pa4_encfs <Key Phrase> <Mirror Directory> <Mount Point> 
	if(argc < 4) {
		fprintf(stderr, "Not enough arguments.\nUsage: ./pa4_encfs <Key Phrase> <Mirror Directory> <Mount Point>\n");
		return 1;
	}

	fsState = malloc(sizeof(fs_state));
	if(fsState == NULL) {
		perror("Failure during memory allocation.\n");
		abort();
	}

	//Stores the path and encryption key phrase in struct. realpath() is root dir arguement.
	fsState -> rootdir = realpath(argv[argc - 2], NULL);
	fsState -> key = argv[argc - 3]; 

	//Rearrange command line arguments to pass them into fuse_main */
	argv[argc - 3] = argv[argc - 1];
	argv[argc - 2] = NULL;
	argv[argc - 1] = NULL;
	argc -= 2;

	return fuse_main(argc, argv, &pa4_encfs_oper, fsState);
}
