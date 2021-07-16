
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mpi.h>
#include <stdlib.h>
#include <unistd.h> /* for execl */

void check( int rc )
{
    if (rc != MPI_SUCCESS )
        abort();
}

int main( int argc, char ** argv)
{
    /* If this program is given an argument, it is interpreted as
     * the path to MPIRUN, which will then be used to reexecute itself
     * but then without any parameters.
     */
    if (argc > 1) {
        char * mpirun = argv[1];
        char * is_thread_compat = argv[0];
        char * const new_argv[] = 
            { mpirun, is_thread_compat, NULL};
        execv( mpirun, new_argv );
        return EXIT_FAILURE;
    }

    char buf[100];
    MPI_Win window = MPI_WIN_NULL;
    int mpi_wins_are_unified = 1;
    int * memory_model = 0;
    int flag = 0;

    check(  MPI_Init( &argc, &argv )   );
    check(  MPI_Win_create( buf, sizeof(buf), 1, MPI_INFO_NULL, 
                MPI_COMM_WORLD, &window) );

    check(  MPI_Win_get_attr( window, MPI_WIN_MODEL, &memory_model, &flag ) );
    if (!flag || MPI_WIN_UNIFIED != *memory_model )
        mpi_wins_are_unified = 0;

    MPI_Win_free( &window );

    MPI_Finalize();
    return mpi_wins_are_unified?EXIT_SUCCESS:EXIT_FAILURE;
}
