
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
#include "gtest/gtest.h"

#include <stdint.h>
#include <inttypes.h>

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore args parameter

    lpf_err_t rc = LPF_SUCCESS;
    const size_t MTU = 2048;
    const size_t n = 5*nprocs*MTU;
    size_t i;
    uint64_t * xs, *ys;
    ys = (uint64_t *) malloc( sizeof(ys[0]) * n);
    xs = (uint64_t *) malloc( sizeof(xs[0]) * n);
    for (i = 0; i < n; ++i)
    {
        xs[i] = i*nprocs + pid;
        ys[i] = 0;
    }
        
    rc = lpf_resize_message_queue( lpf, nprocs+1);
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
        EXPECT_EQ( (uint64_t) (i*nprocs+pid), xs[i] );
        EXPECT_EQ( (uint64_t) 0, ys[i] );
    }

    // Each processor copies his row to processor zero.
    {
        size_t start = 5*pid;
        size_t length = 5;
        size_t offset = start - pid;
        rc = lpf_put( lpf, xslot, start*sizeof(xs[0])*MTU,
                    0, yslot, offset*sizeof(xs[0])*MTU, length*sizeof(xs[0])*MTU, 
                    LPF_MSG_DEFAULT );
        EXPECT_EQ( LPF_SUCCESS, rc );
    }

        
    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    if ( 0 == pid )
    {
        /**
         *     The array 'xs' is sent to processor 0 as a rooftiling. Writes
         *     will overlap at the end and beginning of each tile. On those
         *     places a specific sequential ordering has to be inplace.
         *
         *     proc0 proc1 proc2 proc3
         *     01234 56789 ..... .....
         *       |     /
         *       v    |
         *     01234  v
         *         56789
         *             .....
         *                 .....
         *
         *     Note that the arithmetic can get screwed up if the numbers grow
         *     too big.
         **/

        unsigned int offset = 0;
        for (i = 0; i < nprocs; ++i)
        {
            int j;
            size_t k;
            for (j = 0; j < 5 ; ++j)
            {
                uint64_t fromPid = ys[(4*i+j) * MTU] - xs[(4*i + j + offset) * MTU];
                fromPid = (fromPid + nprocs*MTU)%nprocs;
                for (k = 0; k < MTU; ++k)
                {
                    uint64_t fromPid2 = ys[ (4*i+j) * MTU + k] 
                        - xs[(4*i+j + offset) * MTU + k];
                    fromPid2 = (fromPid2 + nprocs*MTU)%nprocs;
                    EXPECT_EQ( fromPid, fromPid2 );

                    if (fromPid == i) {
                        EXPECT_EQ( (5*i+j)*nprocs*MTU + fromPid, ys[(4*i+j)*MTU] );
		    }
                }

                if (0 == j && i > 0)
                {
                    EXPECT_EQ( 1, fromPid == i || fromPid == i-1 );
                }
                else if (4 == j && i < nprocs-1)
                {
                    EXPECT_EQ( 1, fromPid == i || fromPid == i+1 );
                }
                else
                {
                    EXPECT_EQ( fromPid, (uint64_t) i );
                }
            }
            offset += 1;
        }    
        EXPECT_EQ( (unsigned) nprocs, offset );
        // the rest of the ys array should be zero
        for (i = 0; i < (nprocs-1) * MTU ; ++i)
        {
            EXPECT_EQ( (uint64_t) 0, ys[n-i-1]);
        }
    }
    else
    {
        // on the other processors nothing has changed
        for (i = 0; i < n; ++i)
        {
            EXPECT_EQ( (uint64_t) ( i*nprocs + pid), xs[i] );
            EXPECT_EQ( (uint64_t) 0, ys[i] );
        }
    }

    rc = lpf_deregister( lpf, xslot );
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_deregister( lpf, yslot );
    EXPECT_EQ( LPF_SUCCESS, rc );
}

/** 
 * \test Test lpf_put writing to partial overlapping memory areas from multiple processors. 
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_lpf_put_parallel_overlapping_rooftiling )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc);
}
