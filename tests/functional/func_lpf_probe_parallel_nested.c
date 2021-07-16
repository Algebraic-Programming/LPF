
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
#include "Test.h"

void spmd2( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    lpf_err_t rc = lpf_resize_message_queue( lpf, nprocs);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, 1 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    lpf_machine_t machine[3] = { LPF_INVALID_MACHINE, LPF_INVALID_MACHINE, LPF_INVALID_MACHINE };
    lpf_memslot_t machineSlot = LPF_INVALID_MEMSLOT ;
    rc = lpf_register_global( lpf, &machine[0], sizeof(machine), &machineSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    if ( 0 == pid )
    {
        machine[0] = ((lpf_machine_t * ) args.input)[0];
        machine[1] = ((lpf_machine_t * ) args.input)[1];
        EXPECT_EQ( "%zd", args.input_size, 2*sizeof(lpf_machine_t) );
    }
    else
    {
        // broadcast machine info
        rc = lpf_get( lpf, 0, machineSlot, 0, machineSlot, 0, 2*sizeof(machine[0]), LPF_MSG_DEFAULT );
        EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    }
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    

    rc = lpf_probe( lpf, &machine[2] );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    EXPECT_EQ( "%u", machine[0].p, machine[1].p );
    EXPECT_EQ( "%u", machine[0].p, machine[2].p );
    EXPECT_EQ( "%u", 1u, machine[2].free_p );
    EXPECT_LT( "%g", 0.0, (*(machine[2].g))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine[2].l))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine[2].g))(machine[0].p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine[2].l))(machine[0].p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine[2].g))(machine[0].p, (size_t)(-1), LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine[2].l))(machine[0].p, (size_t)(-1), LPF_SYNC_DEFAULT) );

    rc = lpf_deregister( lpf, machineSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
}

void spmd1( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore any arguments passed through call to lpf_exec

    lpf_pid_t p = 0;
    lpf_machine_t subMachine;
    lpf_err_t rc = lpf_probe( lpf, &subMachine );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    rc = lpf_resize_message_queue( lpf, nprocs);
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_resize_memory_register( lpf, 1 );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    lpf_machine_t machine ;
    lpf_memslot_t machineSlot = LPF_INVALID_MEMSLOT ;
    rc = lpf_register_global( lpf, &machine, sizeof(machine), &machineSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    if ( 0 == pid )
    {
        machine = * ( lpf_machine_t * ) args.input;
        EXPECT_EQ( "%zd", args.input_size, sizeof(lpf_machine_t) );
    }
    else
    {
        // broadcast machine info
        rc = lpf_get( lpf, 0, machineSlot, 0, machineSlot, 0, sizeof(machine), LPF_MSG_DEFAULT );
        EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    }
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    rc = lpf_deregister( lpf, machineSlot );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
    

    // Do some checks
    EXPECT_EQ( "%u", nprocs, subMachine.p / 2 );
    EXPECT_EQ( "%u", nprocs, machine.p / 2 );
    EXPECT_LT( "%g", 0.0, (*(subMachine.g))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(subMachine.l))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(subMachine.g))(machine.p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(subMachine.l))(machine.p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(subMachine.g))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(subMachine.l))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT) );

    const int pthread = 1, mpirma = 1, mpimsg = 1, hybrid = 0, ibverbs=1; 
    (void) pthread; (void) mpirma; (void) mpimsg; (void) hybrid; (void) ibverbs;
    if (LPF_CORE_IMPL_ID) // this part is disabled for the hybrid implementation, because
    {                     // that one doesn't do generic nesting of lpf_exec's
        EXPECT_EQ( "%d", 1,  subMachine.free_p == 2 || subMachine.free_p == 3 );

        // compute the sum of all 'free_p' values
        lpf_pid_t * vec = malloc(sizeof(lpf_pid_t)*nprocs);
        EXPECT_NE( "%p", NULL, vec );
        vec[ pid ] = subMachine.free_p;

        lpf_memslot_t vecSlot = LPF_INVALID_MEMSLOT;
        rc = lpf_register_global( lpf, vec, sizeof(lpf_pid_t)*nprocs, &vecSlot);
        EXPECT_EQ( "%d", LPF_SUCCESS, rc );
        rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
        EXPECT_EQ( "%d", LPF_SUCCESS, rc );
        for (p = 0 ; p < nprocs; ++p)
        {
            if ( pid != p )
            {
                rc = lpf_put( lpf, 
                        vecSlot, pid*sizeof(vec[0]), 
                        p, vecSlot, pid*sizeof(vec[0]), sizeof(vec[0]), LPF_MSG_DEFAULT );
                EXPECT_EQ( "%d", LPF_SUCCESS, rc );
            }
        }
        rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
        EXPECT_EQ( "%d", LPF_SUCCESS, rc );
        rc = lpf_deregister( lpf, vecSlot );
        lpf_pid_t sum = 0;
        for (p = 0; p < nprocs; ++p)
        {
            sum += vec[p];
        }
        EXPECT_EQ( "%u", sum, machine.p );
        EXPECT_EQ( "%u", sum, subMachine.p );
    
        free(vec);
    }

    // When running this spmd1 section, only half of the processes was started
    // This time we try to run spmd2 with a number of processes depending on the
    // pid. Of course, only max free_p processes are started.
    lpf_machine_t multiMachine[2] = { machine, subMachine };
    args.input = multiMachine;
    args.input_size = sizeof(multiMachine);
    rc = lpf_exec( lpf, pid + 3, &spmd2, args );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );
}


/** 
 * \test Test lpf_probe function on a parallel section where all processes are used immediately.
 * \note Extra lpfrun parameters: -probe 1.0
 * \pre P >= 2
 * \return Exit code: 0
 */
TEST( func_lpf_probe_parallel_nested )
{
    lpf_err_t rc = LPF_SUCCESS;

    lpf_machine_t machine;

    rc = lpf_probe( LPF_ROOT, &machine );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    EXPECT_LE( "%u", 1u, machine.p );
    EXPECT_LE( "%u", 1u, machine.free_p );
    EXPECT_LE( "%u", machine.p, machine.free_p );
    EXPECT_LT( "%g", 0.0, (*(machine.g))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.l))(1, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.g))(machine.p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.l))(machine.p, 0, LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.g))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT) );
    EXPECT_LT( "%g", 0.0, (*(machine.l))(machine.p, (size_t)(-1), LPF_SYNC_DEFAULT) );

    lpf_args_t args;
    args.input = &machine;
    args.input_size = sizeof(machine);
    args.output = NULL;
    args.output_size = 0;
    args.f_symbols = NULL;
    args.f_size = 0;

    rc = lpf_exec( LPF_ROOT, machine.p / 2, &spmd1, args );
    EXPECT_EQ( "%d", LPF_SUCCESS, rc );

    return 0;
}
