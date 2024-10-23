
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
#include <stdlib.h>

// Disable automatic initialization.
const int LPF_MPI_AUTO_INITIALIZE = 0;

/**
 * \test Tests lpf_hook on mpi implementation using TCP/IP to initialize, while
 * a timeout happens and _exit() is not called right away. \pre P >= 100 \pre P
 * <= 100 \return Exit code: 1
 */
TEST(func_lpf_hook_tcp_timeout_mpi) {
  MPI_Init(NULL, NULL);

  int pid, nprocs;
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &pid);

  lpf_err_t rc = LPF_SUCCESS;
  lpf_init_t init;
  rc =
      lpf_mpi_initialize_over_tcp("localhost", "9325", 999, pid, nprocs, &init);

  EXPECT_EQ("%d", rc, LPF_ERR_FATAL);

  return 0;
}
