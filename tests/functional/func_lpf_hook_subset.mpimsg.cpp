
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

#include "Test.h"
#include <lpf/core.h>
#include <lpf/mpi.h>

#include <mpi.h>

const int LPF_MPI_AUTO_INITIALIZE = 0;

void test_spmd(lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args) {
  (void)ctx;
  (void)pid;
  (void)nprocs;
  (void)args;
  return;
}

void subset_func(MPI_Comm comm) {
  MPI_Barrier(comm);

  lpf_init_t init;
  lpf_err_t rc = lpf_mpi_initialize_with_mpicomm(comm, &init);
  EXPECT_EQ("%d", LPF_SUCCESS, rc);

  rc = lpf_hook(init, test_spmd, LPF_NO_ARGS);
  EXPECT_EQ("%d", LPF_SUCCESS, rc);
}

/**
 * \test Test for lpf_hook on mpi implementation when only using a subset
 * \pre P >= 3
 * \return Exit code: 0
 */
TEST(func_lpf_hook_subset) {
  MPI_Init(NULL, NULL);

  int s;
  MPI_Comm_rank(MPI_COMM_WORLD, &s);

  int subset =
      s < 2; // Processes are divided into 2 subsets {0,1} and {2,...,p-1}

  MPI_Comm subset_comm;
  MPI_Comm_split(MPI_COMM_WORLD, subset, s, &subset_comm);

  // only the first subset enters that function
  if (subset) {
    subset_func(subset_comm);
  }

  MPI_Barrier(MPI_COMM_WORLD); // Paranoid barrier

  MPI_Finalize();
  return 0;
}
