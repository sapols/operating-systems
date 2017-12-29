#include "sharedArray.h"

#define ARRAY_SIZE 15 


int arrayIsEmpty(array* a) {
    if((a->front == a->rear) && (a->array[a->front].value == NULL)){
	return 1;
    }
    else{
	return 0;
    }
}

int arrayIsFull(array* a) {
    if((a->front == a->rear) && (a->array[a->front].value != NULL)){
	return 1;
    }
    else{
	return 0;
    }
}

int arrayInit(array* a, int size) {
    a->maxSize = size;
    
    a->array = malloc(sizeof(array_node)*(a->maxSize));
    if(!(a->array)){	
	perror("Error on array Malloc");
	return 1;
    }

    int i;
    for(i=0; i < a->maxSize; i++) {
	a->array[i].value = NULL;
    }

    a->front = 0;
    a->rear = 0;

    return a->maxSize;
}

int pushToArray(array* a, void* newValue){
    
    if(arrayIsFull(a)){
	return 1;
    }

    a->array[a->rear].value = newValue;
    a->rear = ((a->rear+1) % a->maxSize);

    return 0;
}

void* popFromArray(array* a){
    void* returnValue;
	
    if(arrayIsEmpty(a)){
	return NULL;
    }
	
    returnValue = a->array[a->front].value;
    a->array[a->front].value = NULL;
    a->front = ((a->front + 1) % a->maxSize);

    return returnValue;
}

void freeArrayMemory(array* a)
{
    while(!arrayIsEmpty(a)){
	popFromArray(a);
    }
    free(a->array);
}

