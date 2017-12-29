#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 15 


typedef struct array_node_struct {
    void* value;
} array_node;

typedef struct array_struct {
    array_node* array;
    int front;
    int rear;
    int maxSize;
} array;


int arrayInit(array* a, int size);

int arrayIsEmpty(array* a);

int arrayIsFull(array* a);

int pushToArray(array* a, void* value);

void* popFromArray(array* a);

void freeArrayMemory(array* a);

