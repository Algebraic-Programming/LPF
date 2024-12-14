
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

#include "mpi.h"
#include <lpf/core.h>
#include <lpf/noc.h>
#include "gtest/gtest.h"

void spmd( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
    (void) args; // ignore args parameter

    lpf_err_t rc = LPF_SUCCESS;

    char buf1[30] = {'\0'};
    char buf2[30] = {'\0'};

    strcpy(buf1, "HELLO");

    rc = lpf_resize_memory_register(lpf, 2); // identical to lpf_noc_resize at the moment
    EXPECT_EQ( LPF_SUCCESS, rc );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_resize_message_queue( lpf, 2);
    EXPECT_EQ( LPF_SUCCESS, rc );

    rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
    EXPECT_EQ( LPF_SUCCESS, rc );

    lpf_memslot_t xslot = LPF_INVALID_MEMSLOT;
    lpf_memslot_t yslot = LPF_INVALID_MEMSLOT;
    rc = lpf_register_local( lpf, buf1, sizeof(buf1), &xslot );
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_noc_register( lpf, buf2, sizeof(buf2), &yslot );
    EXPECT_EQ( LPF_SUCCESS, rc );

       
    int left = (nprocs + pid - 1) % nprocs;
    int right = ( pid + 1) % nprocs;

    char * buffer;
    size_t bufferSize; 
    lpf_serialize_slot(lpf, yslot, &buffer, &bufferSize);
    char rmtBuff[bufferSize];

    MPI_Sendrecv(buffer, bufferSize, MPI_BYTE, left, 0, rmtBuff, bufferSize, MPI_BYTE, right, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    rc = lpf_deserialize_slot(lpf, rmtBuff, yslot);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_noc_put(lpf, xslot, 0, right, yslot, 0, sizeof(buf1), LPF_MSG_DEFAULT);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_sync(lpf, LPF_SYNC_DEFAULT);
    EXPECT_EQ( LPF_SUCCESS, rc );
    EXPECT_EQ(std::string(buf2), std::string(buf1));
    rc = lpf_deregister(lpf, xslot);
    EXPECT_EQ( LPF_SUCCESS, rc );
    rc = lpf_noc_deregister(lpf, yslot);
    EXPECT_EQ( LPF_SUCCESS, rc );

}

/** 
 * \test Testing NOC functionality
 * \pre P >= 2
 * \pre P <= 2
 * \return Exit code: 0
 */
TEST( API, func_lpfAPI_test_noc_ring )
{
    lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, spmd, LPF_NO_ARGS);
    EXPECT_EQ( LPF_SUCCESS, rc );
}
