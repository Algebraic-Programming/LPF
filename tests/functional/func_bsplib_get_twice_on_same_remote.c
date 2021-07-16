
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

#include <stdint.h>

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, 0, &bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );


    char x = 'x';
    char y = 'y';
    char z = 'z';

    rc = bsplib_push_reg(bsplib, &x, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    rc = bsplib_push_reg(bsplib, &y, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    if ( bsplib_pid(bsplib) == 0 )
        rc = bsplib_get(bsplib, 1, &x, 0, &y, sizeof( x ) );
    else if ( bsplib_pid(bsplib) == 1 )
        rc = bsplib_get(bsplib, 0, &y, 0, &z, sizeof( y ) );

    EXPECT_EQ( "%c", 'x', x );
    EXPECT_EQ( "%c", 'y', y );
    EXPECT_EQ( "%c", 'z', z );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    EXPECT_EQ( "%c", 'x', x );
    EXPECT_EQ( "%c", bsplib_pid(bsplib) == 0 ? 'x' : 'y', y );
    EXPECT_EQ( "%c", bsplib_pid(bsplib) == 1 ? 'y' : 'z', z );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    // redo the previous but now the order of pid 0 and 1 reversed

    x = 'x';
    y = 'y';
    z = 'z';

    rc = bsplib_push_reg(bsplib, &x, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    rc = bsplib_push_reg(bsplib, &y, sizeof( x ) );
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    if ( bsplib_pid(bsplib) == 1 )
        rc = bsplib_get(bsplib, 0, &x, 0, &y, sizeof( x ) );
    else if ( bsplib_pid(bsplib) == 0 )
        rc = bsplib_get(bsplib, 1, &y, 0, &z, sizeof( y ) );

    EXPECT_EQ( "%c", 'x', x );
    EXPECT_EQ( "%c", 'y', y );
    EXPECT_EQ( "%c", 'z', z );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );

    EXPECT_EQ( "%c", 'x', x );
    EXPECT_EQ( "%c", bsplib_pid(bsplib) == 1 ? 'x' : 'y', y );
    EXPECT_EQ( "%c", bsplib_pid(bsplib) == 0 ? 'y' : 'z', z );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( "%d", BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests two lpf_gets where one memory location serves as source and destination for two gets
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( func_bsplib_get_twice_on_same_remote )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    return 0;
}

