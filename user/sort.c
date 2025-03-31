/******************************************************************************
* 
* sort.c
*
* This program implements a randomized quicksort and can be used as a
* hypothetical workload. This algorithm is in-place, it does not declare
* any sub-arrays that need to be allocated during program execution. 
*
* Usage: This program takes a single input describing the size of the array
*        to sort. 
*
* Written Sept 7, 2015 by David Ferry
******************************************************************************/

#include <stdio.h>  //For printf()
#include <stdlib.h> //For exit(), atoi(), and rand()
#include <assert.h> //For assert()

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <paging.h>
#define PAGE_SIZE sysconf(_SC_PAGESIZE)

const int num_expected_args = 2;

static void *
mmap_malloc(int    fd,
            size_t bytes)
{

    void * data;

    data = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Could not mmap " DEV_NAME ": %s\n", strerror(errno));
        return NULL;
    }

    return data;

    //return malloc(bytes);
}

// Uncomment the following #define statement to turn on verification that the
// array is sorted. If the array is correct, the program will exit normally.
// If the array is not sorted, it will abort with an assert statement.
//#define VERIFY_CORRECT

//This function swaps the values pointed to by a and b.
void swap( double *a, double* b ){
	unsigned temp;

	temp = *a;
	*a = *b;
	*b = temp;
}

// This function implements the pivot selection, and partitions the array slice
// between start and end such that all elements before the pivot are less than
// and all elements after the pivot are greater than. This function returns
// the location of the pivot.
unsigned partition(double *A, unsigned start, unsigned end){
	unsigned pivot, pivot_val, range, target, index;
	range = end - start;
	pivot = (rand() % range) + start;

	//Swap the pivot into the last position
	swap(&A[pivot], &A[end]);

	//Now we walk through this partition
	pivot_val = A[end];
	target = start;
	for( index = start; index < end; index++ ){
		if( A[index] <= pivot_val ){
		swap( &A[target], &A[index] );
		target++;
		}
	}
	swap( &A[target], &A[end] ); 

	return target;
}

void quicksort( double *A, unsigned start, unsigned end ){
	
	unsigned pivot;

	if( start < end ){
		pivot = partition(A, start, end);
		if( pivot > start )
		quicksort(A, start, pivot - 1);
		if( pivot < end )
		quicksort(A, pivot + 1, end);
	}
}


void error_quit( double *A, unsigned end, unsigned location){
	unsigned index;
	printf( "Error located at %u\n", location );
	for( index = 0; index < end; index++)
		printf("%u %0.3f\n",index, A[index]);
	abort();
}


int main( int argc, char* argv[] ){

	unsigned index; //loop indicies
	unsigned array_size;
	double *A;
	int fd;

	fd = open(DEV_NAME, O_RDWR);
    	if (fd == -1) {
        	fprintf(stderr, "Could not open " DEV_NAME ": %s\n", strerror(errno));
        	return -1;
    	}

	if( argc != num_expected_args ){
		printf("Usage: ./sort <size of array to sort>\n");
		exit(-1);
	}

	array_size = atoi(argv[1]);
	
	printf("Generating array...\n");
	A = (double*) mmap_malloc(fd, sizeof(double) * array_size );

	for( index = 0; index < array_size; index++ ){
		A[index] = (double) rand();
	}

	printf("Sorting array...\n");

	quicksort( A, 0, array_size);

	#ifdef VERIFY_CORRECT
	printf("Verifying array is sorted...\n");
	for( index = 0; index < (array_size - 1); index++)
		if( !(A[index] <= A[index + 1]) ){
			//Array is not sorted
			error_quit(A, array_size, index);
		}
	#endif 

	printf("Sort done!\n");

	return 0;
}
