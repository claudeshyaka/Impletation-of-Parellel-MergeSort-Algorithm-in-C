
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Specifies the cut-off size of the array before it switches from
// parallel merges/sorts to a serial implementation
#define THRESHOLD 512

#define TRUE 1
#define FALSE 0

///////////////////////////////////////////////////////////////////////////////
//                             Type Declarations                             //
///////////////////////////////////////////////////////////////////////////////

// Contains the arguments that get passed to the pthread_sort functions
typedef struct
{
  long *result;
  long *source;
  long size;
} SortArg_t;

// Represents the arguments passed as part of the merge process
typedef struct
{
  long *result;
  long *array_b;
  long *array_c;
  long b_size;
  long c_size;
} MergeArg_t;

///////////////////////////////////////////////////////////////////////////////
//                             Global Variables                              //
///////////////////////////////////////////////////////////////////////////////

// manage access to the active thread_count
pthread_mutex_t mutex_;

// Running count of the number of threads within the threads array that are currently running
long thread_count_ = 0;

// Stores the size of the thread pool that is specified from command line
long THREAD_MAX_NUM = 0;

///////////////////////////////////////////////////////////////////////////////
//                             Function Prototypes                           //
///////////////////////////////////////////////////////////////////////////////
long pthread_binary_search( long *search_array, long array_size, long value );
void pthread_s_merge( long *result, long *array_b, long b_size, long *array_c, long c_size );
void* pthread_p_merge( void* args );
void* pthread_merge_sort( void *args );
void internal_pthread_join( long thread_index );
void internal_pthread_create(void *(*start_routine) (void *), void* args, long *p_thread_index);
int initialize_threads( int num_of_threads );
int cleanup_threads();
long *pthread_sort(long *array, long size, int num_of_threads);
int increment_thread_count();
void decrement_thread_count();

///////////////////////////////////////////////////////////////////////////////
//                          Function Implementation                          //
///////////////////////////////////////////////////////////////////////////////
long pthread_binary_search( long *search_array, long array_size, long value )
{

  long min = 0;
  long max = array_size - 1;
  long mid = 0;

  if(value <= search_array[0])
  {
    return 0;
  }
  if(value >= search_array[array_size - 1])
  {
    return array_size;
  }

  while(min < max)
  {
    mid = (min + max) / 2;
    if(search_array[mid-1] <= value && search_array[mid] >= value)
    {
      return mid;
    }
    else if(search_array[mid] <= value && search_array[mid+1] >= value)
    {
      return mid + 1;
    }

    if(search_array[mid] < value)
    {
      min = mid + 1;
    }
    else if(search_array[mid] > value)
    {
      max = mid - 1;
    }
    else
    {
      return mid;
    }
  }

  return -1;
}

void pthread_s_merge( long *result, long *array_b, long b_size, long *array_c, long c_size )
{

  while( b_size > 0 && c_size > 0 )
  {
    if(*array_b <= *array_c)
    {
      *result++ = *array_b++; b_size--;
    }
    else
    {
      *result++ = *array_c++; c_size--;
    }
  }

  while( b_size > 0 )
  {
    *result++ = *array_b++; b_size--;
  }

  while( c_size > 0 )
  {
    *result++ = *array_c++; c_size--;
  }

}

void* pthread_p_merge( void* args )
{
  MergeArg_t *pMergeArgs = (MergeArg_t*)args;
  pthread_t thread_ctx;
  int needs_cleanup = FALSE;

  // invert the array that is considered B as B needs to be the larger one
  if(pMergeArgs->b_size < pMergeArgs->c_size)
  {
    MergeArg_t merge_args;
    merge_args.result  = pMergeArgs->result;
    merge_args.array_b = pMergeArgs->array_c;
    merge_args.b_size  = pMergeArgs->c_size;
    merge_args.array_c = pMergeArgs->array_b;;
    merge_args.c_size  = pMergeArgs->b_size;
    pthread_p_merge( (void*)&merge_args );
  }
  else if( pMergeArgs->b_size <= THRESHOLD )
  {
    // perform sequential merge rather than parallel
    pthread_s_merge( pMergeArgs->result, pMergeArgs->array_b, pMergeArgs->b_size, pMergeArgs->array_c, pMergeArgs->c_size );
  }
  else
  {
    long mid_index = pMergeArgs->b_size / 2;
    long bin_index = pthread_binary_search( pMergeArgs->array_c, pMergeArgs->c_size, pMergeArgs->array_b[mid_index] );
    if(bin_index < 0)
    {
      printf("ERROR: Received Invalid Binary Search Result\n");
      exit(1);
    }   
    pMergeArgs->result[mid_index + bin_index] = pMergeArgs->array_b[mid_index];

    // handle values less than the mid_index value within B
    MergeArg_t left_args;
    left_args.result  = pMergeArgs->result;
    left_args.array_b = pMergeArgs->array_b;
    left_args.b_size  = mid_index;
    left_args.array_c = pMergeArgs->array_c;
    left_args.c_size  = bin_index;
    if(increment_thread_count() == TRUE)
    {
      pthread_create( &thread_ctx, NULL, &pthread_p_merge, &left_args );
      needs_cleanup = TRUE;
    }
    else
    {
      // The thread pool is full and thus execution needs to continue within this thread
      // so that work is being completed. A deadlock situation can take place if a forced
      // spawn of a thread is necessary. This is particularly likely if the number of allowed
      // threads is small while the source array is large.
      pthread_p_merge((void*)&left_args);
    }

    // handle values larger than the mid_index value within B
    MergeArg_t right_args;
    right_args.result  = pMergeArgs->result + mid_index + bin_index + 1;
    right_args.array_b = pMergeArgs->array_b + mid_index + 1;
    right_args.b_size  = pMergeArgs->b_size - mid_index - 1;
    right_args.array_c = pMergeArgs->array_c + bin_index;
    right_args.c_size  = pMergeArgs->c_size - bin_index;
    pthread_p_merge( (void*)&right_args );

    // Need to wait for the thread created to handle the left half of problem
    if(needs_cleanup == TRUE)
    {
      pthread_join( thread_ctx, NULL );
      decrement_thread_count();
    }
  }

  return NULL;
}

long pthread_partition( long *buffer, long start, long end )
{

  long temp = 0;

  long pivot = buffer[end];
  long i = start - 1;
  long j = start;
  for( j = start; j < end; j++ )
  {
    if(buffer[j] <= pivot)
    {
      i++;
      temp = buffer[i];
      buffer[i] = buffer[j];
      buffer[j] = temp;
    }
  }
  temp = buffer[i+1];
  buffer[i+1] = buffer[end];
  buffer[end] = temp;

  return i + 1;
}

void pthread_recursive_quicksort( long *buffer, long start, long end )
{
  if(start >= end)
  {
    return;
  }

  long mid = pthread_partition( buffer, start, end );
  pthread_recursive_quicksort( buffer, start, mid - 1 );
  pthread_recursive_quicksort( buffer, mid + 1, end );
}

void pthread_quicksort( long *result, long *source, long size )
{
  // the following sort algorithm will perform an in place
  // sorting of the data so it needs to be copied to the
  // destination buffer so that they sorting can be performed
  memcpy( result, source, sizeof(long) * size);
  pthread_recursive_quicksort( result, 0, size - 1 );
}

void* pthread_merge_sort( void *args )
{

  SortArg_t *pSortArgs = (SortArg_t*)args;
  pthread_t thread_ctx;
  int needs_cleanup = FALSE;

  if(pSortArgs->size <= THRESHOLD)
  {
    pthread_quicksort( pSortArgs->result, pSortArgs->source, pSortArgs->size );
  }
  else if(pSortArgs->size == 0)
  {
    return NULL;
  }
  else
  {

    long *C = malloc(pSortArgs->size * sizeof(long));
    if(C == 0)
    {
      printf("ERROR: Insufficient Memory; size=%ld\n", pSortArgs->size);
      exit(-1);
    }

    SortArg_t left_args;
    left_args.result = C;
    left_args.source = pSortArgs->source;
    left_args.size   = pSortArgs->size / 2;
    if(increment_thread_count() == TRUE)
    {
      pthread_create( &thread_ctx, NULL, &pthread_merge_sort, &left_args );
      needs_cleanup = TRUE;
    }
    else
    {
      // The thread pool is full and thus execution needs to continue within this thread
      // so that work is being completed. A deadlock situation can take place if a forced
      // spawn of a thread is necessary. This is particularly likely if the number of allowed
      // threads is small while the source array is large.
      pthread_merge_sort((void*)&left_args);
    }

    SortArg_t right_args;
    right_args.result = C + (pSortArgs->size / 2);
    right_args.source = pSortArgs->source + (pSortArgs->size / 2);
    right_args.size   = pSortArgs->size - (pSortArgs->size / 2);
    pthread_merge_sort((void*)&right_args);

    // Need to wait for the thread created to handle the left half of problem
    if(needs_cleanup == TRUE)
    {
      pthread_join( thread_ctx, NULL );
      decrement_thread_count();
    }

    MergeArg_t merge_args;
    merge_args.result  = pSortArgs->result;
    merge_args.array_b = C;
    merge_args.b_size  = (pSortArgs->size / 2);
    merge_args.array_c = C + (pSortArgs->size / 2);
    merge_args.c_size  = pSortArgs->size - (pSortArgs->size / 2);
    pthread_p_merge( (void*)&merge_args );

    free(C);
  }

  return NULL;
}

int increment_thread_count()
{
  int ret_val = TRUE;
  pthread_mutex_lock( &mutex_ );
  if(thread_count_ >= THREAD_MAX_NUM)
  {
    ret_val = FALSE;
  }
  else
  {
    thread_count_ += 1;
    ret_val = TRUE;
  }
  pthread_mutex_unlock( &mutex_ );
  return ret_val;
}

void decrement_thread_count()
{
  pthread_mutex_lock(&mutex_);
  thread_count_ -= 1;
  pthread_mutex_unlock(&mutex_);
}

int initialize_threads( int num_of_threads )
{

  // Step 1. Need to initialize the mutex for protecting access to thread_count and
  //         thread pool
  if(pthread_mutex_init(&mutex_, NULL) != 0)
  {
    printf("ERROR: Failed to initialize mutex\n");
    return FALSE;
  }

  // Step 2. Reset active thread count
  thread_count_ = 0;

  // Step 3. Configure the thread pool size global
  THREAD_MAX_NUM = num_of_threads;

  return TRUE;
}

int cleanup_threads()
{

  // Step 1. Destroy the mutex
  if(pthread_mutex_destroy(&mutex_) != 0)
  {
    printf("ERROR: Failed to destroy the mutex properly\n");
    return FALSE;
  }

  // Step 2. Reset the thread pool size and active counts
  thread_count_  = 0;
  THREAD_MAX_NUM = 0;

  return TRUE;
}

long *pthread_sort(long *array, long size, int num_of_threads) 
{
  int error = FALSE;

  // attempt to initialize all the resources necessary to manage
  // the specified thread pool size
  if(!initialize_threads(num_of_threads))
  {
    printf("ERROR: Failed to inialize memory system\n");
    return array;
  }

  long *result = malloc(sizeof(long) * size);
  if(result == 0)
  {
    printf("Insufficient Memory\n");
    error = TRUE;
  }

  if(error == FALSE)
  {
    SortArg_t args;
    args.result = result;
    args.source = array;
    args.size   = size;
    pthread_merge_sort( (void*)&args );
  }

  if(!cleanup_threads())
  {
    printf("ERROR: Failed to release resources from thread pool\n");
  }

  if(error == FALSE)
    return result;
  else
    return array;
}
