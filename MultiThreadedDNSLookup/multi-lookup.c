#include "multi-lookup.h"


//Mutex locks
pthread_mutex_t arrayLock; 
pthread_mutex_t requestorLock; //Protects counter in "readFile".
pthread_mutex_t resolverLock; //Protects results.txt in "dns".

//Condition Variables
pthread_cond_t arrayFull; //Wait/Signal for full array.
pthread_cond_t arrayEmpty; //Wait/Signal for empty array. 

array sharedArray; //The shared array.
int numFilesCompleted; 
int numInputFiles;	
int maxThreads;	
int numRequestorThreads;
int numResolverThreads;
FILE* servicedfp;
char* servicedFile; //serviced.txt
char* resultsFile; //results.txt


/*
 * (Currently only works if number of requestor threads equals number of input files.)
 *
 * Create requestor threads to service name files until numRequestorThreads have been created.
 */
void* requestorThreadPool(char* inputFiles) {
	//Pointer magic suppresses the warning: "initialization makes pointer from integer without a cast [-Wint-conversion]"
	char** inputFileNames = (char**) inputFiles;
	
	pthread_t requestorThreads[numRequestorThreads]; //Declare specified number of requestor threads

	int i;
	for(i=0; i < numRequestorThreads; i++) {
		char* fileName = inputFileNames[i];
		pthread_create(&requestorThreads[i], NULL, (void*) checkAndReadFiles, (void*) fileName);
		//Wait until threads have finished. 
		pthread_join(requestorThreads[i], NULL);
	}

	for(i=0; i < numInputFiles; i++) {
		pthread_cond_signal(&arrayEmpty); //Signal requestor threads that the array is empty.
	}

	return NULL;
}

/*
 * Create resolver threads to do dns lookups until numResolverThreads have been created.
 */
void* resolverThreadPool(FILE* resultsfp) {
	pthread_t resolverThreads[numResolverThreads]; //Declare specified number of resolver threads
	
	int i;
	for(i=0; i < numResolverThreads; i++) {
		pthread_create(&resolverThreads[i], NULL, dns, resultsfp);
		//Wait until threads have finished. 
		pthread_join(resolverThreads[i], NULL);
	}

	return NULL;
}

/*
 * Calls readFile() for requestor threads (and will handle logic of threads servicing multiple files).
 */
void* checkAndReadFiles(char* filename) {
	int filesServicedByThread = 1;
	//(Check if this thread can keep servicing files here?)
	readFile(filename, filesServicedByThread);
	
	return NULL;
}

/* 
 * Read names from input files and put them into the shared array.
 */
void readFile(char* filename, int filesServicedByThread) {
	//int namesServiced = 0; //Count number of names a thread services (unused).

	char filePath[1025] = "input/";
	strcat(filePath, filename);
	 
	FILE* inputfp = fopen(filePath, "r"); //Open file for reading
	//Error Handling #3: "Bogus Input File Path"
	if(!inputfp) {
		perror("Error: cannot open input file.\n");
		//Lock counter variable  
		pthread_mutex_lock(&requestorLock);
			numFilesCompleted++;
		//Unlock counter variable
		pthread_mutex_unlock(&requestorLock);
	}

	char hostname[1025];
	while(fscanf(inputfp, "%1024s", hostname) > 0) {
		//Lock and check to see if array is full. 
		pthread_mutex_lock(&arrayLock);
			while(arrayIsFull(&sharedArray)) { 
				pthread_cond_wait(&arrayFull, &arrayLock); //If the array is full, thread waits on CV "arrayFull".
			}
			pushToArray(&sharedArray, strdup(hostname));
			//namesServiced++;
			pthread_cond_signal(&arrayEmpty); //Signal on condition arrayEmpty to let resolver threads know that shared array is populated.
		// unlock shared array  
		pthread_mutex_unlock(&arrayLock);
	}
 
	pthread_mutex_lock(&requestorLock);
		numFilesCompleted++;	
		fprintf(servicedfp, "Thread <%ld> serviced %d file(s).\n", syscall( __NR_gettid ), filesServicedByThread);
	pthread_mutex_unlock(&requestorLock);
	
	fclose(inputfp);
}

/* 
 * Do the dns lookups and write results to results.txt
 */
void* dns(FILE* resultsfp) {
	while(1) {
		//Lock array mutex to access it
		pthread_mutex_lock(&arrayLock);
			while(arrayIsEmpty(&sharedArray)) {
				pthread_mutex_lock(&requestorLock); //Lock access to numFilesCompleted
					int finished = 0;
	
					if(numFilesCompleted == numInputFiles) finished = 1; //if we are at the end of the array

				pthread_mutex_unlock(&requestorLock); //Unlock numFilesCompleted
	
				if (finished) { 
		//Unlock array mutex because we're finished
		pthread_mutex_unlock(&arrayLock);
					return NULL;
				}
				//If the array is empty but there are still files to service, resolver threads must wait on CV arrayEmpty.
				pthread_cond_wait(&arrayEmpty, &arrayLock);
			}

			char* hostname = (char*) popFromArray(&sharedArray);
			pthread_cond_signal(&arrayFull); //Let resolver threads know there is space in the array.
		
			char firstIp[MAX_IP_LENGTH];

			//Error Handling #1: "Bogus Hostname" 
			if (dnslookup(hostname, firstIp, sizeof(firstIp))==UTIL_FAILURE){
				fprintf(stderr, "Error: DNS lookup failure for hostname: %s\n", hostname);
				strncpy(firstIp, "", sizeof(firstIp));
			}

			pthread_mutex_lock(&resolverLock); //Lock results.txt
				fprintf(resultsfp, "%s,%s\n", hostname, firstIp);
			pthread_mutex_unlock(&resolverLock); //Unlock results.txt
		
			free(hostname);
		 
		pthread_mutex_unlock(&arrayLock);
	}

	return NULL;
}

int main(int argc, char* argv[]) {
	numFilesCompleted = 0;
	numInputFiles = argc-5;
	numRequestorThreads = atoi(argv[1]); 
	numResolverThreads = atoi(argv[2]);
	resultsFile = argv[3]; 
	servicedFile = argv[4]; 
	char* inputFiles[numInputFiles];

        //Get time of day
	struct timeval start, end;
	gettimeofday(&start, NULL);
	
	printf("Allocating %d requestor thread(s).\n", numRequestorThreads);	
	printf("Allocating %d resolver thread(s).\n", numResolverThreads);
	
	arrayInit(&sharedArray, ARRAY_SIZE);

	//Initialize condition variables
	pthread_cond_init(&arrayFull, NULL);
	pthread_cond_init(&arrayEmpty, NULL);

	//Initialize mutex locks.
	pthread_mutex_init(&arrayLock, NULL);
	pthread_mutex_init(&requestorLock, NULL);
	pthread_mutex_init(&resolverLock, NULL);	

	if (argc < MIN_ARGS){
		fprintf(stderr, "%d arguments is not enough arguments. \n", (argc-1));
		fprintf(stderr, "USAGE:\n %s %s\n", argv[0], USAGE);
		return(EXIT_FAILURE);
	}

	//Create an array of input files. 
	int i;
	for(i=0; i < numInputFiles; i++) {
		inputFiles[i] = argv[i+5]; //+5 because argv[0] is program name, 1 & 2 are # threads, 3 is results.txt, 4 is serviced.txt
	}

	servicedfp = fopen(servicedFile, "w"); //open serviced.txt for writing
	//Error Handling #2: "Bogus Output File Path" 
	if(!servicedfp) {
		perror("Error: unable to open 'serviced' file. ");
		exit(EXIT_FAILURE);
	}
	
	FILE* resultsfp = fopen(resultsFile, "w"); //open results.txt for writing
	//Error Handling #2: "Bogus Output File Path" 
	if(!resultsfp) {
		perror("Error: unable to open 'results' file. ");
		exit(EXIT_FAILURE);
	}

	//Declare requestor ID thread to initialize requestor thread pool.
	pthread_t requestorID;

	int requestor = pthread_create(&requestorID, NULL, (void*) requestorThreadPool, inputFiles);
	if (requestor != 0) {
		errno = requestor;
		perror("Error: (pthread_create for requestor) ");
		exit(EXIT_FAILURE);
	}

	//Declare resolver ID thread to initialize resolver thread pool.
	pthread_t resolverID;
	
	int resolver = pthread_create(&resolverID, NULL, (void*) resolverThreadPool, resultsfp);
	if (resolver != 0) {
		errno = resolver;
		perror("Error: (pthread_create for resolver) ");
		exit(EXIT_FAILURE);
	}

	//(Wait for threads to finish using pthread_join; it suspends execution of a calling thread until execution is complete.)
	pthread_join(requestorID, NULL);
	pthread_join(resolverID, NULL);

	fclose(servicedfp);
	fclose(resultsfp);
	freeArrayMemory(&sharedArray);

	//Destroy mutex locks
	pthread_mutex_destroy(&resolverLock);
	pthread_mutex_destroy(&arrayLock);
	pthread_mutex_destroy(&requestorLock);

	//Destroy condition variables
	pthread_cond_destroy(&arrayEmpty);
	pthread_cond_destroy(&arrayFull);

	gettimeofday(&end, NULL);
	printf("Time taken: %ld sec.\n", (end.tv_sec - start.tv_sec));
	
	pthread_exit(NULL);
	return EXIT_SUCCESS;
}
