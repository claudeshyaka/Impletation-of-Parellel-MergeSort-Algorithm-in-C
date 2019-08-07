#!/bin/sh

#$ -cwd
#$ -j yes
#$ -o sort.out
#$ -N clw_sort_proj1
./sort 10000000

