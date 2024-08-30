
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
#include <math.h>

#include "gtest/gtest.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore args parameter

    lpf_err_t rc = LPF_SUCCESS;
    const unsigned n = sqrt(nprocs);
    unsigned i;
    unsigned * xs, *ys;
    ys = (unsigned *) malloc( sizeof(ys[0]) * n);
    xs = (unsigned *) malloc( sizeof(xs[0]) * n);
    for (i = 0; i < n; ++i)
    {
        xs[i] = i;
        ys[i] = 0;
    }
        
    rc = lpf_resize_message_queue( lpf, n);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, 2 );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );
 
    lpf_memslot_t xslot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t yslot = LPF_INVALID_MEMSLOT;
    rc = lpf_register_local( lpf, xs, sizeof(xs[0]) * n, &xslot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_register_global( lpf, ys, sizeof(ys[0]) * n, &yslot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT);
    EXPECT_EQ( LPF_SUCCESS, rc );

    // Check that data is OK.
    for (i = 0; i < n; ++i)
    {
        EXPECT_EQ( i, xs[i] );
        EXPECT_EQ( 0u, ys[i] );
    }

    if ( pid < n )
    {
        for ( i = 0; i < n; ++ i)
        {
            EXPECT_LT( i*n, nprocs);
            rc = lpf_put( lpf, xslot, sizeof(xs[0])*i, 
                    i*n, yslot, sizeof(ys[0])*pid, sizeof(xs[0]), 
                    LPF_MSG_DEFAULT );
            EXPECT_EQ( LPF_SUCCESS, rc );
        }
    }

        
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    for (i = 0; i < n; ++i)
    {
        EXPECT_EQ( i, xs[i] );
        if ( pid % n == 0 && pid < n*n)
            EXPECT_EQ( pid / n, ys[i] );
        else
            EXPECT_EQ( 0, ys[i] );
    }

}

/** 
 * \test Test lpf_put by doing a pattern which bad for a sparse all-to-all
 * \pre P >= 16
 * \pre P <= 16
 * \return Exit code: 0
 */
TEST( API, func_lpf_put_parallel_bad_pattern )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}
