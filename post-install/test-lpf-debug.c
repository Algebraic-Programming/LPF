
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
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
    unsigned i;
    lpf_resize_message_queue( lpf, nprocs/2 );
    lpf_resize_memory_register( lpf, 1 );

    int broadcast = 0;
    if (pid == 0) broadcast = 10 - (int) pid;

    lpf_sync( lpf, LPF_SYNC_DEFAULT );

    lpf_memslot_t bc_slot = LPF_INVALID_MEMSLOT;
    lpf_register_global( lpf, &broadcast, sizeof(broadcast), &bc_slot);

    if (pid != 0) 
        lpf_get( lpf, 0, bc_slot, 0, bc_slot, 0, sizeof(broadcast), LPF_MSG_DEFAULT );

    lpf_sync( lpf, LPF_SYNC_DEFAULT );
   
}

int main( int argc, char ** argv )
{
    lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS );
    return EXIT_SUCCESS;
}
