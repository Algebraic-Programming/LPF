
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

    int a[1000];
    memset( &a, 0, sizeof(a) );
    int n = sizeof(a)/sizeof(a[0]);
    int i;


    while (1)
    {
        rc = bsplib_create( lpf, pid, nprocs, 1, 0, &bsplib);
        EXPECT_EQ( BSPLIB_SUCCESS, rc );

        for ( i = 0; i < n; ++i ) {
            rc = bsplib_push_reg( bsplib, a, i );
            EXPECT_EQ( BSPLIB_SUCCESS, rc );
        }

        rc = bsplib_sync( bsplib );

        if (rc == BSPLIB_SUCCESS )
        {
            break;
        }
        else if (rc == BSPLIB_ERR_OUT_OF_MEMORY )
        {
            // reinitialize BSPlib
            rc = bsplib_destroy( bsplib);
            EXPECT_EQ( BSPLIB_SUCCESS, rc );

            // reduce number of registers
            n /= 2;
        }
        else
        {
            break;
        }
    } 
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    for ( i = 0; i < n; ++i ) {
        rc = bsplib_pop_reg( bsplib, &a );
        EXPECT_EQ( BSPLIB_SUCCESS, rc );
    }
 
    rc = bsplib_pop_reg( bsplib, &a );
    EXPECT_EQ( BSPLIB_ERR_MEMORY_NOT_REGISTERED, rc );

    rc = bsplib_sync( bsplib );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Performance test data-structure for managing memory registrations
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_pushpopreg_many_same)
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

