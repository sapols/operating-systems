#include <pthread.h>	
#include <stdlib.h>		
#include <stdio.h>		
#include <errno.h>
#include <unistd.h>		
#include <sys/syscall.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include "util.h"	
#include "sharedArray.h"	

#define MIN_ARGS 6
#define USAGE "<numRequestorThreads> <numResolverThreads> <resultsFile> <servicedFile> <inputFiles>"
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define MIN_RESOLVER_THREADS 2

void* checkAndReadFiles(char* filename);

void readFile(char* filename, int filesServicedByThread);

void* requestorThreadPool(char* inputFiles);

void* resolverThreadPool();

void* dns();

int main(int argc, char* argv[]);

