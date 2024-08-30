
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

    EXPECT_EQ( "%d", 2, nprocs );
    if ( 0 == pid )
    {
        EXPECT_EQ( "%d", 1, * (int *) args.input );
        *(int *) args.output = 1;
    }
    else
    {
        EXPECT_EQ( "%zd", (size_t) 0, args.input_size );
        EXPECT_EQ( "%zd", (size_t) 0, args.output_size );
        EXPECT_EQ( "%p", (void *) NULL, args.input );
        EXPECT_EQ( "%p", (void *) NULL, args.output );
    }
}


/** 
 * \test Test single lpf_exec() call with a single arg on two processors
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( func_lpf_exec_single_call_single_arg_dual_proc )
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
    rc = lpf_exec( LPF_ROOT, 2, &spmd, args );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    EXPECT_EQ( "%d", 1, output );
    return 0;
}
