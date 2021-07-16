
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

#include "dynamichook.hpp"
#include "time.hpp"
#include "assert.hpp"
#include <gtest/gtest.h>

#include <mpi.h>

#include <iostream>

int main(int argc, char ** argv)
{
    MPI_Init(&argc, &argv);
    ASSERT( argc >= 6 && "usage: ./dynamichook.t [server] [port] [pid] [nprocs] [timeout]");

    std::string server = argv[1];
    std::string port = argv[2];
    int pid = atoi(argv[3]);
    int nprocs = atoi(argv[4]);
    int timeout = atoi(argv[5]);

    MPI_Comm comm = MPI_COMM_NULL;
    
    try {
        comm = lpf::mpi::dynamicHook( server, port, pid, nprocs, 
            lpf::Time::fromSeconds( timeout / 1000.0 ) );
    }
    catch( std::runtime_error & e)
    {
        ADD_FAILURE() << "hookup failed. Fatal!: " << e.what() << "\n";
        _exit(EXIT_FAILURE);
    }

    int mpiPid = -1, mpiNprocs = -1;
    MPI_Comm_rank( comm, &mpiPid);
    MPI_Comm_size( comm, &mpiNprocs );

    EXPECT_EQ( pid, mpiPid );
    EXPECT_EQ( nprocs, mpiNprocs );

    MPI_Comm_free(&comm);

    MPI_Finalize();

    return 0;
}
