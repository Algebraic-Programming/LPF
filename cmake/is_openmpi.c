#include "mpi.h"
#include <stdio.h>

#ifdef OMPI_MAJOR_VERSION

int main() {
    printf("The OMPI_MAJOR_VERSION is %d\n", OMPI_MAJOR_VERSION);
    return 0;
}
#else
#error This is not Open MPI
#endif
