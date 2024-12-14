
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

#include "ibverbsNoc.hpp"
#include "mpilib.hpp"
#include <string.h>
#include "gtest/gtest.h"


using namespace lpf::mpi;

extern "C" const int LPF_MPI_AUTO_INITIALIZE=0;


/** 
 * \test Testing NOC functionality
 * \pre P >= 2
 * \pre P <= 2
 * \return Exit code: 0
 */
TEST( API, func_verbsAPI_zero_test_noc_ring )
{

    char buf1[30] = {'\0'};
    char buf2[30] = {'\0'};

    strcpy(buf1, "HELLO");

    MPI_Init(NULL, NULL);
    Lib::instance();
    Comm * comm = new Comm();
    *comm = Lib::instance().world();
    int rank = comm->pid();
    assert(comm->nprocs() > 0);
    comm->barrier();
    IBVerbsNoc * verbs = new IBVerbsNoc( *comm );
    
    verbs->resizeMemreg(3);
    comm->barrier();
    
    verbs->resizeMesgq( 2 );
    comm->barrier();

    IBVerbs::SlotID b1 = verbs->regLocal( buf1, sizeof(buf1) );
    IBVerbs::SlotID b2 = verbs->regNoc( buf2, sizeof(buf2) );

    auto mr = verbs->getMR(b1, rank);
    mr = verbs->getMR(b2, rank);
    assert(mr._addr != nullptr);
    char * buffer;
    size_t bufSize = mr.serialize(&buffer);
    std::string bufAsString(buffer);
       
    int left = (comm->nprocs() + rank - 1) % comm->nprocs();
    int right = (rank + 1) % comm->nprocs();
    char rmtBuff[bufSize];
    std::stringstream ss(buffer);

    MPI_Sendrecv(buffer, bufSize, MPI_BYTE, left, 0, rmtBuff, bufSize, MPI_BYTE, right, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MemoryRegistration * newMr = MemoryRegistration::deserialize(rmtBuff);
    verbs->setMR(b2, right, *newMr);
    comm->barrier();
    verbs->put( b1, 0, right, b2, 0, sizeof(buf1));
    verbs->sync(true);
    EXPECT_EQ(std::string(buf2), std::string(buf1));
    verbs->dereg(b1);
    verbs->dereg(b2);
    delete verbs;
    delete comm;
    MPI_Finalize();
}
