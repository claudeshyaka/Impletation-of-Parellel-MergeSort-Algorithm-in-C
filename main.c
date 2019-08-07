/**
 * Copyright (c) 2015 I-Ting Angelina Lee (angelee@wustl.edu)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk_api.h>

#include "ktiming.h"

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

#define TIMING_COUNT 5
// #define TIMING_COUNT 1

static unsigned long rand_nxt = 0;

static inline unsigned long my_rand(void)
{

  rand_nxt = rand_nxt * 1103515245 + 12345;
  return rand_nxt;
}

static inline void my_srand(unsigned long seed)
{

  rand_nxt = seed;
}

#define swap(a, b) \
  {                \
    long tmp;      \
    tmp = a;       \
    a = b;         \
    b = tmp;       \
  }

static void scramble_array(long *arr, unsigned long size)
{

  unsigned long i;
  unsigned long j;

  for (i = 0; i < size; ++i)
  {
    j = my_rand();
    j = j % size;
    swap(arr[i], arr[j]);
  }
}

static void fill_array(long *arr, unsigned long size, long start)
{

  unsigned long i;

  my_srand(1);
  /* first, fill with integers 1..size */
  for (i = 0; i < size; ++i)
  {
    arr[i] = i + start;
  }

  /* then, scramble randomly */
  scramble_array(arr, size);
}

static void
check_result(long *res, unsigned long size, long start, char *name)
{
  printf("Now check result ... \n");
  int success = 1;
  for (unsigned long i = 0; i < size; i++)
  {
    if (res[i] != (start + i))
      success = 0;
  }
  if (!success)
    fprintf(stdout, "%s sorting FAILURE!\n", name);
  else
    fprintf(stdout, "%s sorting successful.\n", name);
}

/* forward declaration */
long *cilk_sort(long *array, unsigned long size);
long *pthread_sort(long *array, unsigned long size, int thread_count);

void call_cilk_sort(long *array, unsigned long size, long start, int check)
{
  clockmark_t begin, end;
  uint64_t elapsed_time[TIMING_COUNT];
  long *cilk_res = NULL;

  for (int i = 0; i < TIMING_COUNT; i++)
  {
    /* calling the sort implemented using cilk */
    begin = ktiming_getmark();
    cilk_res = cilk_sort(array, size);
    end = ktiming_getmark();
    elapsed_time[i] = ktiming_diff_usec(&begin, &end);

    if (check && i == 0)
    {
      check_result(cilk_res, size, start, "cilk_sort");
    }
    // free the array if not the same
    if (array != cilk_res)
    {
      free(cilk_res);
    }
    scramble_array(array, size);
  }

  print_runtime(elapsed_time, TIMING_COUNT);
}

void call_pthread_sort(long *array, unsigned long size, long start, int check, int thread_count)
{
  clockmark_t begin, end;
  uint64_t elapsed_time[TIMING_COUNT];
  long *pthread_res = NULL;

  for (int i = 0; i < TIMING_COUNT; i++)
  {
    /* calling the sort implemented using cilk */
    begin = ktiming_getmark();
    pthread_res = pthread_sort(array, size, thread_count);
    end = ktiming_getmark();
    elapsed_time[i] = ktiming_diff_usec(&begin, &end);

    if (check && i == 0)
    {
      check_result(pthread_res, size, start, "pthread_sort");
    }
    // free the array if not the same
    if (array != pthread_res)
    {
      free(pthread_res);
    }
    scramble_array(array, size);
  }

  print_runtime(elapsed_time, TIMING_COUNT);
}

int main(int argc, char **argv)
{

  /* default benchmark options */
  int check = 1;
  unsigned long size = 10000000;
  int thread_count = 1;
  long *array;

  if (argc < 3)
  {
    if (argc == 1 && argv[0][0] != '\0')
    {
      fprintf(stderr, "Usage: %s <n> <n>\n", argv[0]);
    }
    else
    {
      fprintf(stderr, "Usage: ./sort <n> <n>\n");
    }
    exit(0);
  }
  // Size of the array.
  size = atol(argv[1]);
  
  // number of threads
  thread_count = atol(argv[2]);

  long start = my_rand();

  fprintf(stdout, "Creating a randomly permuted array of size %ld.\n", size);
  array = (long *)malloc(size * sizeof(long));
  fill_array(array, size, start);

  call_cilk_sort(array, size, start, check);
  __cilkrts_end_cilk();
  call_pthread_sort(array, size, start, check, thread_count);

  free(array);

  return 0;
}
