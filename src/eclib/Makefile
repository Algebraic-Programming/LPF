CFLAGS=-I$(HOME)/libfabric/include -D_GNU_SOURCE -g -Wall -Wextra
LINK_FLAGS=-L$(HOME)/libfabric/lib -lm -lfabric -fsanitize=address,undefined
#LINK_FLAGS=-L$(HOME)/libfabric/lib -lfabric

MPICC=$(HOME)/packages/mpich-install/bin/mpicc

my_pingpong: ec.c ec.h pingpong.c pingpong.h MPI_PP.c perfs.c perfs.h
	$(MPICC) $(CFLAGS) perfs.c ec.c pingpong.c MPI_PP.c -o my_pingpong $(LINK_FLAGS) -Wunused-function
