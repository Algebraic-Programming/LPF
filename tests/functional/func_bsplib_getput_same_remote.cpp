
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
#include <lpf/bsplib.h>
#include "gtest/gtest.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, -1, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    char x = 'x';
    char y = 'y';
    char z = 'z';

    rc = bsplib_push_reg(bsplib, &x, sizeof( x ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_get(bsplib, 0, &x, 0, &z, sizeof( x ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_put(bsplib, 0, &z, &x, 0, sizeof( x ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    EXPECT_EQ( 'x', x );
    EXPECT_EQ( 'y', y );
    EXPECT_EQ( 'z', z );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    EXPECT_EQ( bsplib_pid(bsplib) == 0 ? 'z' : 'x', x );
    EXPECT_EQ( 'y', y );
    EXPECT_EQ( 'x', z );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests a common case of a bsplib_get
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_getput_same_remote )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

