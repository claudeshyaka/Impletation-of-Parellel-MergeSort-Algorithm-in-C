CC=/project/cec/class/cse539/tapir/build/bin/clang
CXX=/project/cec/class/cse539/tapir/build/bin/clang++
CILK_LIBS=/project/linuxlab/gcc/6.4/lib64

CFLAGS = -ggdb -O3 -fcilkplus
# CILK_LIBS = -L/project/cec/class/cse539_sp15/gcc/lib64 
LIBS = -L$(CILK_LIBS) -Wl,-rpath -Wl,$(CILK_LIBS) -lcilkrts -lpthread
PROGS = sort

all:: $(PROGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ -c $<

sort: pthread_sort.o cilk_sort.o main.o ktiming.o
	$(CXX) -o $@ $^ $(LIBS)

clean::
	-rm -f $(PROGS) *.o

