
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
#include "Test.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 0, 0, &bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    char x = 'x';
    char y = 'y';
    char z = 'z';

    rc = bsplib_push_reg(bsplib, &x, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    rc = bsplib_push_reg(bsplib, &z, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    rc = bsplib_get(bsplib, 0, &x, 0, &z, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    rc = bsplib_put(bsplib, 0, &y, &z, 0, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    EXPECT_EQ( "%c", 'x', x );
    EXPECT_EQ( "%c", 'y', y );
    EXPECT_EQ( "%c", 'z', z );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    EXPECT_EQ( "%c", 'x', x );
    EXPECT_EQ( "%c", 'y', y );
    EXPECT_EQ( "%c", bsplib_pid(bsplib) == 0 ? 'y' : 'x', z );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests a bsplib_get and a lpf_put to the same destination in unsafe mode
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( func_bsplib_getput_same_dest_unsafemode )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    return 0;
}

