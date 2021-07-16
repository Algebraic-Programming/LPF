
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

#include <lpf/bsplib.h>

#include <stdint.h>
#include <string.h>

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    bsplib_err_t rc = BSPLIB_SUCCESS;
    
    bsplib_t bsplib;
    rc = bsplib_create( lpf, pid, nprocs, 1, 0, &bsplib);

    int i, j;
    int n = 5;
    int result = 0, p = bsplib_nprocs(bsplib);
    int local_sums[p];
    int xs[n];
    memset( local_sums, 0, sizeof( local_sums ) );

    for ( j = 0; j < n; ++j )
        xs[j] = j + bsplib_pid(bsplib);

    // All-set. Now compute the sum

    for ( j = 0; j < n; ++j )
        result += xs[j];

    rc = bsplib_push_reg(bsplib, &result, sizeof( result ) );
    rc = bsplib_sync(bsplib);

    for ( i = 0; i < p; ++i ) {
        rc = bsplib_hpget(bsplib, i, &result, 0, &local_sums[i], sizeof( int ) );
    }
    rc = bsplib_sync(bsplib);

    result = 0;
    for ( i = 0; i < p; ++i )
        result += local_sums[i];
    rc = bsplib_pop_reg(bsplib, &result );

    rc = bsplib_destroy( bsplib);
}

/** 
 * \test Tests an example from Hill's BSPlib paper
 * \pre P >= 1
 * \return Exit code: 0
 */
int main()
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS );
    
    return 0;
}


