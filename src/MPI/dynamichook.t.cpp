
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

extern "C" const int LPF_MPI_AUTO_INITIALIZE=0;
int myArgc;
char **myArgv;

/** 
 * \pre P >= 1
 * \pre P <= 1
 * \return Exit code: 1
 */

int main(int argc, char ** argv)
{
    myArgc = argc;
    myArgv = argv;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();

}

TEST(API, dynamicHook)
{

  /**
   * This test is run via following options via
   * ./dynamichook.t <server> <port> <pid> <nprocs> <timeout>
   * Being run "as is" without all these 5 arguments will lead it
   * to fail (which is the normal Google Test case, so we expect exit code 1)
   * However, when run via custom add_test tests with correct 5 arguments, it shall work
   */
  ASSERT_GE(myArgc, 6);
  MPI_Init(&myArgc, &myArgv);
  std::string server = myArgv[1];
  std::string port = myArgv[2];
  int pid = atoi(myArgv[3]);
  int nprocs = atoi(myArgv[4]);
  int timeout = atoi(myArgv[5]);


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
}

