
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
    rc = bsplib_create( lpf, pid, nprocs, 1, (size_t) -1, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    int a;
    // Use of variable without definition
    rc = bsplib_put( bsplib, 0, &a, &a, 0, sizeof( a ) );
    EXPECT_EQ( BSPLIB_ERR_MEMORY_NOT_REGISTERED, rc );
        
    // Tests use of put directly after registration before sync
    rc = bsplib_push_reg( bsplib, &a, sizeof( a ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_put( bsplib, 0, &a, &a, 0, sizeof( a ) );
    EXPECT_EQ( BSPLIB_ERR_MEMORY_NOT_REGISTERED, rc );
    rc = bsplib_sync( bsplib );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_put( bsplib, 0, &a, &a, 0, sizeof( a ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_pop_reg( bsplib, &a );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_sync( bsplib );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    // Tests detection of NULL ptr
    rc = bsplib_push_reg( bsplib, NULL, 1);
    EXPECT_EQ( BSPLIB_ERR_NULL_POINTER, rc );

    // Tests deregistration of non-existent registration
    rc = bsplib_pop_reg( bsplib, &a );
    EXPECT_EQ( BSPLIB_ERR_MEMORY_NOT_REGISTERED, rc );


    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests whether push_reg and pop_reg can detect various illegal uses.
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_pushpopreg_exception )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

