
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
    rc = bsplib_create( lpf, pid, nprocs, 0, 0, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    int n = 5 * bsplib_nprocs(bsplib);
    int i, dst_pid, dst_idx, p = bsplib_nprocs(bsplib), n_over_p = n / p;

    int xs[n_over_p];
    for ( i = 0; i < n_over_p; ++i )
    {
        xs[i] = n - ( i + bsplib_pid(bsplib) * n_over_p ) - 1;
    }

    EXPECT_EQ( 0, n % p );
    rc = bsplib_push_reg(bsplib, xs, n_over_p * sizeof( int ) );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    for ( i = 0; i < n_over_p; ++i )
    {
        dst_pid = xs[i] / n_over_p;
        dst_idx = xs[i] % n_over_p;
        rc = bsplib_put(bsplib, dst_pid, 
                &xs[i], xs, dst_idx * sizeof( int ),
            sizeof( int ) );
        EXPECT_EQ( BSPLIB_SUCCESS, rc );
    }
    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    rc = bsplib_pop_reg(bsplib, xs );
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    for ( i = 0; i < n_over_p; ++i )
    {
        EXPECT_EQ( i + (int) bsplib_pid(bsplib) * n_over_p, xs[i] );
    }


    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
}

/** 
 * \test Tests the put array example from Hill's BSPlib paper in unsafe mode
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_put_array_unsafemode )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

