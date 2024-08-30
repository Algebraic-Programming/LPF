
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
#include <math.h>

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    size_t maxhpregs = (size_t) -1;
   
    const int pthread = 1, mpirma = 2, mpimsg = 3, hybrid = 4, ibverbs=5; 
    (void) pthread; (void) mpirma; (void) mpimsg; (void) hybrid; (void) ibverbs;
    if (LPF_CORE_IMPL_ID == mpirma )
    {                     
        maxhpregs = 10; // because MPI RMA only supports a limited number
                        // of memory registrations
    }
 
    rc = bsplib_create( lpf, pid, nprocs, 1, maxhpregs, &bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    int i, j;
    size_t size;
    const int m = 100;
    const int n = m*(m+1)/2;
    uint32_t * memory = (uint32_t *) malloc( 2 + n *sizeof(uint32_t) );
    uint32_t *array = memory + 2;
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    for ( i = 0; i < n; ++i )
    {
        memory[i] = 0xAAAAAAAAu;
    }

    uint32_t value[m];
    for (i = 0; i < m; ++i)
    {
        value[i] = 0x12345678;
    }

    size = bsplib_set_tagsize( bsplib, sizeof(j));
    EXPECT_EQ( (size_t) 0, size);

    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );


    for (i = 1, j=0; i <= m; j += i, ++i) {
        rc = bsplib_hpsend(bsplib, 
                ( bsplib_pid(bsplib) + 1 ) % bsplib_nprocs(bsplib),
                &j, value, i*sizeof( uint32_t ) );
        EXPECT_EQ( BSPLIB_SUCCESS, rc );
    }


    rc = bsplib_sync(bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );

    const void * tag, *payload;
    for ( i = 1; i <= m; ++i) {
        size = bsplib_hpmove( bsplib, &tag, &payload);
        EXPECT_NE( (size_t) -1, size );
        memcpy( &j, tag, sizeof(j));
        double size_approx  = (1 + sqrt(1 + 8*j))/2;
        size_t k = (size_t) (size_approx + 0.5*(1.0 - 1e-15));

        EXPECT_EQ( k*sizeof(uint32_t), size );
        memcpy( array + j, payload, sizeof(uint32_t)*k);
    }
    size =bsplib_hpmove( bsplib, &tag, &payload);
    EXPECT_EQ( (size_t) -1, size );
    
    for ( i = 0; i < n; ++i )
    {
        if ( i < 2)
        {
            EXPECT_EQ( 0xAAAAAAAAu, memory[i] );
        }
        else
        {
            EXPECT_EQ( 0x12345678u, memory[i] );
        }
    }

    for ( i = 0; i < m; ++i ) {
        EXPECT_EQ( 0x12345678u, value[i] );
    }

    rc = bsplib_destroy( bsplib);
    EXPECT_EQ( BSPLIB_SUCCESS, rc );
    free(memory);
}

/** 
 * \test Tests sending a lot of messages through bsp_hpsend
 * \pre P >= 1
 * \return Exit code: 0
 */
TEST( API, func_bsplib_hpsend_many)
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}

