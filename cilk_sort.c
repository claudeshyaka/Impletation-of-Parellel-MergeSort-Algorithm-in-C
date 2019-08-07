#include <cilk/cilk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Specifies the cut-off size of the array before it switches from
// parallel merges/sorts to a serial implementation
#define THRESHOLD 512

///////////////////////////////////////////////////////////////////////////////
//                          Function Implementation                          //
///////////////////////////////////////////////////////////////////////////////


long binary_search( long *search_array, long array_size, long value )
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

void s_merge( long *result, long *array_b, long b_size, long *array_c, long c_size )
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

void p_merge( long *result, long *array_b, long b_size, long *array_c, long c_size )
{

  // invert the array that is considered B as B needs to be the larger one
  if(b_size < c_size)
  {
    p_merge( result, array_c, c_size, array_b, b_size );
  }
  else if( b_size <= THRESHOLD )
  {
    // perform sequential merge rather than parallel
    s_merge( result, array_b, b_size, array_c, c_size );
  }
  else
  {
    long mid_index = b_size / 2;
    long bin_index = binary_search( array_c, c_size, array_b[mid_index] );
    if(bin_index < 0)
    {
      printf("ERROR: Received Invalid Binary Search Result\n");
      exit(1);
    }
    
    result[mid_index + bin_index] = array_b[mid_index];

    // handle values less than the mid_index value within B
    cilk_spawn p_merge( result, array_b, mid_index, array_c, bin_index );

    // handle values larger than the mid_index value within B
    p_merge( result + mid_index + bin_index + 1, array_b + mid_index + 1, b_size - mid_index - 1, array_c + bin_index, c_size - bin_index);

    // wait for all spawned threads to be completed
    cilk_sync;
  }

}

long cilk_partition( long *buffer, long start, long end )
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

void cilk_recursive_quicksort( long *buffer, long start, long end )
{
  if(start >= end)
  {
    return;
  }

  long mid = cilk_partition( buffer, start, end );
  cilk_recursive_quicksort( buffer, start, mid - 1 );
  cilk_recursive_quicksort( buffer, mid + 1, end );
}

void cilk_quicksort( long *result, long *source, long size )
{
  // the following sort algorithm will perform an in place
  // sorting of the data so it needs to be copied to the
  // destination buffer so that they sorting can be performed
  memcpy( result, source, sizeof(long) * size);
  cilk_recursive_quicksort( result, 0, size - 1 );
}

void MergeSort( long *result, long *source, long size ){

  if(size <= THRESHOLD )
  {
    cilk_quicksort( result, source, size );
  }
  else if(size == 0)
  {
    return;
  }
  else
  {

    long *C = malloc(size * sizeof(long));
    if(C == 0)
    {
      printf("ERROR: Insufficient Memory; size=%ld\n", size);
      exit(-1);
      return;
    }

    cilk_spawn MergeSort(C, source, size / 2);
    MergeSort(C + (size / 2), source + (size / 2), size - (size / 2));
    cilk_sync;

    p_merge( result, C, (size / 2), C + (size / 2), size - (size / 2));
    

    free(C);
  }

}

long *cilk_sort(long *array, long size) {

  long *result = malloc(sizeof(long) * size);
  if(result == 0)
  {
    printf("Insufficient Memory\n");
    exit(-1);
  }

  MergeSort( result, array, size );
  
  return result;
}

