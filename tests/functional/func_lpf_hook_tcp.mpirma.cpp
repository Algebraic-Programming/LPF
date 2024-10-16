
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
#include "Test.h"

#include <stdlib.h>
#include <mpi.h>

void spmd( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    lpf_err_t rc = LPF_SUCCESS;

    struct { int pid, nprocs; } params;
    EXPECT_EQ( "%lu", sizeof(params), args.input_size );

    memcpy( &params, args.input, sizeof(params));
    EXPECT_EQ( "%u", (lpf_pid_t) params.pid, pid );
    EXPECT_EQ( "%u", (lpf_pid_t) params.nprocs, nprocs );

    rc = lpf_resize_message_queue( ctx, 2);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( ctx, 2);
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );
    rc = lpf_sync(ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    int x = 5 - pid;
    int y = pid;

    lpf_memslot_t xSlot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t ySlot = LPF_INVALID_MEMSLOT;

    rc = lpf_register_global( ctx, &x, sizeof(x), &xSlot );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );
    rc = lpf_register_global( ctx, &y, sizeof(y), &ySlot );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    rc = lpf_put( ctx, xSlot, 0, (pid + 1) % nprocs, ySlot, 0, sizeof(x), LPF_MSG_DEFAULT );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    rc = lpf_sync( ctx, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    EXPECT_EQ( "%d", x, (int) (5 - pid) );
    EXPECT_EQ( "%d", y, (int) (5 - (pid + nprocs -1) % nprocs) );
}

// disable automatic initialization.
const int LPF_MPI_AUTO_INITIALIZE=0; 

/** 
 * \test Tests lpf_hook on mpi implementation using TCP/IP to initialize. The pids and nprocs are checked for their correctness.
 * \pre P >= 1
 * \return Exit code: 0
 * \note Independent processes: yes
 */
TEST( func_lpf_hook_tcp )
{
    lpf_err_t rc = LPF_SUCCESS;
    MPI_Init(&argc, &argv);

    struct { int pid, nprocs; } params = { 0, 0};
    EXPECT_GT("%d", argc, 2 );
    params.pid = atoi( argv[1] );
    params.nprocs = atoi( argv[2] );

    lpf_init_t init;
    rc = lpf_mpi_initialize_over_tcp( 
            "localhost", "9325", 240000, // time out should be high in order to
            params.pid, params.nprocs, &init); // let e.g. Intel MPI try a few
                                               // alternative fabrics

    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    lpf_args_t args;
    args.input = &params;
    args.input_size = sizeof(params);
    args.output = NULL;
    args.output_size = 0;
    args.f_symbols = NULL;
    args.f_size = 0;

    rc = lpf_hook( init, &spmd, args );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    rc = lpf_mpi_finalize( init );
    EXPECT_EQ( "%d", rc, LPF_SUCCESS );

    MPI_Finalize();
    return 0;
}


