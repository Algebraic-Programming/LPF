
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

void function_1(void) {} 
void function_2(int a, long b, double c, float d) 
{ (void) a; (void) b; (void) c; (void) d; }


void spmd1( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) lpf; // ignore lpf context variable

    EXPECT_EQ( "%d", nprocs, 2 );

    if (0 == pid)
    {
        EXPECT_EQ( "%zd", sizeof(int), args.input_size );
        EXPECT_EQ( "%zd", sizeof(int), args.output_size );
        int n = (* (int *) args.input); 
        EXPECT_EQ( "%d", 4, n );
        * (int * ) args.output = 1 ;
    }
    else
    {
        EXPECT_EQ( "%zd", (size_t) 0, args.input_size );
        EXPECT_EQ( "%zd", (size_t) 0, args.output_size );
        EXPECT_EQ( "%p", (void *) NULL, args.input );
        EXPECT_EQ( "%p", (void *) NULL, args.output );
    }
    EXPECT_EQ( "%zd", (size_t) 1 , args.f_size );
    EXPECT_EQ( "%p", (lpf_func_t) function_1, args.f_symbols[0] ); //note: function pointers cannot be formatted in ANSI C
}

void spmd2( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    (void) lpf; // ignore lpf context variable

    EXPECT_EQ( "%d", nprocs, 2 );

    if (0 == pid)
    {
        EXPECT_EQ( "%zd", sizeof(int), args.input_size );
        EXPECT_EQ( "%zd", sizeof(int), args.output_size );
        int n = (* (int *) args.input) ; 
        EXPECT_EQ( "%d", 3, n );
        * (int * ) args.output = 2;
    }
    else
    {
        EXPECT_EQ( "%zd", (size_t) 0, args.input_size );
        EXPECT_EQ( "%zd", (size_t) 0, args.output_size );
        EXPECT_EQ( "%p", (void *) NULL, args.input );
        EXPECT_EQ( "%p", (void *) NULL, args.output );
    }
    EXPECT_EQ( "%zd", (size_t) 1 , args.f_size );
    EXPECT_EQ( "%p", (lpf_func_t) function_2, args.f_symbols[0] ); //note: function pointers cannot be formatted in ANSI C
}




/** 
 * \test Test two lpf_exec() calls with single argument on two processors
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( func_lpf_exec_multiple_call_single_arg_dual_proc )
{
    int input[2] = { 4, 3};
    int output[2] = { -1, -1 };
    lpf_func_t fps[2] = { (lpf_func_t) &function_1, (lpf_func_t) &function_2 };
    lpf_err_t rc = LPF_SUCCESS;
    lpf_args_t args;
    args.input = &input[0];
    args.input_size = sizeof(int);
    args.output = &output[0];
    args.output_size = sizeof(int);
    args.f_symbols = fps;
    args.f_size = 1;

    rc = lpf_exec( LPF_ROOT, 2, &spmd1, args );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    args.input = &input[1];
    args.input_size = sizeof(int);
    args.output = &output[1];
    args.output_size = sizeof(int);
    args.f_symbols = fps + 1;
    args.f_size = 1;

    rc = lpf_exec( LPF_ROOT, 2, &spmd2, args );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    int i;
    for (i = 0; i < 2; ++i)
    {
        int m = input[i];
        EXPECT_EQ( "%d", 4-i, m );
        int n = output[i];
        EXPECT_EQ( "%d", i+1, n );
    }
    return 0;
}
