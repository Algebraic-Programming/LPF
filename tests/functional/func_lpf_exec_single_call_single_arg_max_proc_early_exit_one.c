
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
#include <string.h>
#include "Test.h"



void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) lpf; // ignore lpf context variable
    lpf_err_t rc = LPF_SUCCESS;
    int a[2] = { pid, -1 };
    lpf_memslot_t aSlot = LPF_INVALID_MEMSLOT;

    EXPECT_LE( "%d", 2, nprocs );

    if ( 0 == pid )
    {
        EXPECT_EQ( "%zd", (size_t) sizeof(int), args.input_size );
        EXPECT_EQ( "%zd", (size_t) sizeof(int), args.output_size );
        EXPECT_EQ( "%d", 1, * (int *) args.input );
    }
    else
    {
        EXPECT_EQ( "%zd", (size_t) 0, args.input_size );
        EXPECT_EQ( "%zd", (size_t) 0, args.output_size );
        EXPECT_EQ( "%p", (void *) NULL, args.input );
        EXPECT_EQ( "%p", (void *) NULL, args.output );
    }

    // perform a simple communication
    rc = lpf_resize_message_queue( lpf, 2);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, 1 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_sync( lpf , LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_register_global( lpf, &a, sizeof(a), &aSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync( lpf , LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_put( lpf, aSlot, 0, (pid+1) % nprocs, aSlot, sizeof(a[0]), sizeof(a[0]), LPF_MSG_DEFAULT);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync( lpf , LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    EXPECT_EQ( "%d", a[0], (int) pid );
    EXPECT_EQ( "%d", a[1], (int) ((pid+nprocs-1) % nprocs) );

    rc = lpf_deregister( lpf, aSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    
    // now, all other processes except 'one' perform an extra sync.
    if ( 1 != pid )
    {
        rc = lpf_sync( lpf, LPF_SYNC_DEFAULT);
        EXPECT_EQ( "%d", LPF_ERR_FATAL, rc );
    }
    
    // It is still possible to send output through the args
    if ( 0 == pid )
    {
        *(int *) args.output = 2;
    }
}


/** 
 * \test Test single lpf_exec() call with a single arg on all processors. All processes other than process 1 perform an extra sync.
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( func_lpf_exec_single_call_single_arg_max_proc_early_exit_one )
{
    lpf_err_t rc = LPF_SUCCESS;
    int input = 1;
    int output = 3;
    lpf_args_t args;
    args.input = &input;
    args.input_size = sizeof(int);
    args.output = &output;
    args.output_size = sizeof(int);
    args.f_size = 0;
    args.f_symbols = NULL;
    rc = lpf_exec( LPF_ROOT, LPF_MAX_P, &spmd, args );
    EXPECT_EQ( "%d", LPF_ERR_FATAL , rc );

    EXPECT_EQ( "%d", 2, output );
    return 0;
}
