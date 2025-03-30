/******************************************************************************
* 
* dense_mm.c
* 
* This program implements a dense matrix multiply and can be used as a
* hypothetical workload. 
*
* Usage: This program takes a single input describing the size of the matrices
*        to multiply. For an input of size N, it computes A*B = C where each
*        of A, B, and C are matrices of size N*N. Matrices A and B are filled
*        with random values. 
*
* Written Sept 6, 2015 by David Ferry
* Amended for "Lab 1: Memory Management and Paging" by Brian Kocoloski
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
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
#define ONESEC_INMICROSEC 1000000

const int num_expected_args = 2;
const unsigned sqrt_of_UINT32_MAX = 65536;

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

void print_time_diff(struct timeval * start, struct timeval * end)
{
    time_t tv_sec_diff = (*end).tv_sec - (*start).tv_sec;
    suseconds_t tv_usec_diff = (*end).tv_usec - (*start).tv_usec;

    if(tv_usec_diff < 0)
    {
        tv_usec_diff += ONESEC_INMICROSEC;
	tv_sec_diff--;
    }

    printf("Time Diff: %ld.%06ld\n", (long)tv_sec_diff, (long)tv_usec_diff);
}

int main (int argc, char* argv[])
{
    int fd;
	unsigned index, row, col; //loop indicies
	unsigned matrix_size, squared_size;
	double *A, *B, *C;
	struct timeval tv_start, tv_end;

	if (argc != num_expected_args) {
		printf("Usage: ./dense_mm <size of matrices>\n");
		exit(-1);
	}

	matrix_size = atoi(argv[1]);
	
	if (matrix_size > sqrt_of_UINT32_MAX) {
		printf("ERROR: Matrix size must be between zero and %u!\n", sqrt_of_UINT32_MAX);
		exit(-1);
	}

    fd = open(DEV_NAME, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "Could not open " DEV_NAME ": %s\n", strerror(errno));
        return -1;
    }

	squared_size = matrix_size * matrix_size;
    
    gettimeofday(&tv_start, NULL); 
    A = (double *)mmap_malloc(fd, sizeof(double) * squared_size);
    B = (double *)mmap_malloc(fd, sizeof(double) * squared_size);
    C = (double *)mmap_malloc(fd, sizeof(double) * squared_size);
    gettimeofday(&tv_end, NULL);

    printf("Time for mmap (all 3 matricies):\n");
    print_time_diff(&tv_start, &tv_end);

    gettimeofday(&tv_start, NULL);
	for (row = 0; row < matrix_size; row++) {
		for (col = 0; col < matrix_size; col++) {
			for (index = 0; index < matrix_size; index++){
                C[row*matrix_size + col] += A[row*matrix_size + index] *B[index*matrix_size + col];
			}	
		}
	}
    gettimeofday(&tv_end, NULL);
    
    printf("Multiplication done\n");
    printf("Time for matrix multiplication:\n");
    print_time_diff(&tv_start, &tv_end);

    return 0;
}
