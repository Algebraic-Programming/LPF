
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

#include <stdint.h>

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, 0, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    int i;
    const int n = 10;
    uint32_t memory[n];
    uint32_t *array = memory + 2;
    int length = 5;
    rc = bsplib_push_reg(bsplib, array, length );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    uint32_t value = 0x12345678;
    rc = bsplib_put(bsplib, 
            ( bsplib_pid(bsplib) + 1 ) % bsplib_nprocs(bsplib),
            &value, array, 0,
        sizeof( value ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_pop_reg(bsplib, array );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    for ( i = 0; i < n; ++i )
    {
        memory[i] = 0xAAAAAAAAu;
    }

    for ( i = 0; i < n; ++i )
    {
        EXPECT_EQ( 0xAAAAAAAAu, memory[i] );
    }
    EXPECT_EQ( 0x12345678u, value );

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    for ( i = 0; i < n; ++i )
    {
        if ( 2 != i )
        {
            EXPECT_EQ( 0xAAAAAAAAu, memory[i] );
        }
        else
        {
            EXPECT_EQ( 0x12345678u, memory[i] );
        }
    }
    EXPECT_EQ( 0x12345678u, value );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests a normal lpf_put case. 
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_put_normal)
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

