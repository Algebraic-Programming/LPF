
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

#include <lpf/core.h>
#include <lpf/mpi.h>
#include "gtest/gtest.h"

#include <stdlib.h>
#include <mpi.h>

void spmd( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) args;
    lpf_err_t rc = LPF_SUCCESS;

    rc = lpf_resize_message_queue( ctx, 2);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( ctx, 2);
    EXPECT_EQ( rc, LPF_SUCCESS );
    rc = lpf_sync(ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( rc, LPF_SUCCESS );

    int x = 5 - pid;
    int y = pid;

    lpf_memslot_t xSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t ySlot = LPF_INVALID_MEMSLOT;

    rc = lpf_register_global( ctx, &x, sizeof(x), &xSlot );
    EXPECT_EQ( rc, LPF_SUCCESS );
    rc = lpf_register_global( ctx, &y, sizeof(y), &ySlot );
    EXPECT_EQ( rc, LPF_SUCCESS );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( rc, LPF_SUCCESS );

    rc = lpf_put( ctx, xSlot, 0, (pid + 1) % nprocs, ySlot, 0, sizeof(x), LPF_MSG_DEFAULT );
    EXPECT_EQ( rc, LPF_SUCCESS );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( rc, LPF_SUCCESS );

    EXPECT_EQ( x, (int) (5 - pid) );
    EXPECT_EQ( y, (int) (5 - (pid + nprocs -1) % nprocs) );
}

// disable automatic initialization.
const int LPF_MPI_AUTO_INITIALIZE=0; 

/** 
 * \test Tests lpf_hook on mpi implementation
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST(API, func_lpf_hook_simple_mpi)
{
    lpf_err_t rc = LPF_SUCCESS;
    MPI_Init(NULL, NULL);

    int pid = 0;
    MPI_Comm_rank( MPI_COMM_WORLD, &pid);

    int nprocs = 0;
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs);

    lpf_init_t init;
    rc = lpf_mpi_initialize_with_mpicomm( MPI_COMM_WORLD, &init);
    EXPECT_EQ( rc, LPF_SUCCESS );

    rc = lpf_hook( init, &spmd, LPF_NO_ARGS );
    EXPECT_EQ( rc, LPF_SUCCESS );

    rc = lpf_mpi_finalize( init );
    EXPECT_EQ( rc, LPF_SUCCESS );

    MPI_Finalize();
}


