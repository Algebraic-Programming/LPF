
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

#include "ibverbs.hpp"
#include "assert.hpp"
#include "mpilib.hpp"

#include <gtest/gtest.h>
#include <iostream>

using namespace lpf::mpi;

extern "C" const int LPF_MPI_AUTO_INITIALIZE=0;


/** 
 * \pre P >= 1
 * \pre P <= 2
 */
class IBVerbsTests : public testing::Test {

    protected:

    static void SetUpTestSuite() {

       MPI_Init(NULL, NULL);
        Lib::instance();
        comm = new Comm();
        *comm = Lib::instance().world();
        comm->barrier();
        verbs = new IBVerbs( *comm );
    }

    static void TearDownTestSuite() {
        delete verbs;
        verbs = nullptr;
        delete comm;
        comm = nullptr;
        MPI_Finalize();
    }

    static Comm *comm;
    static IBVerbs *verbs;
};

lpf::mpi::Comm * IBVerbsTests::comm = nullptr;
IBVerbs * IBVerbsTests::verbs = nullptr;


TEST_F( IBVerbsTests, init )
{

    IBVerbs verbs( *comm);
    comm->barrier();
}


TEST_F( IBVerbsTests, resizeMemreg )
{

    verbs->resizeMemreg( 2 );

    comm->barrier();
}


TEST_F( IBVerbsTests, resizeMesgq )
{

    verbs->resizeMesgq( 2 );

    comm->barrier();
}

TEST_F( IBVerbsTests, regVars )
{


    char buf1[30] = "Hi";
    char buf2[30] = "Boe";

    verbs->resizeMemreg( 2 );

    verbs->regLocal( buf1, sizeof(buf1) );
    verbs->regGlobal( buf2, sizeof(buf2) );

    comm->barrier();
}


TEST_F( IBVerbsTests, put )
{

    char buf1[30] = "Hi";
    char buf2[30] = "Boe";

    verbs->resizeMemreg( 2 );
    verbs->resizeMesgq( 1 );

    IBVerbs::SlotID b1 = verbs->regLocal( buf1, sizeof(buf1) );
    IBVerbs::SlotID b2 = verbs->regGlobal( buf2, sizeof(buf2) );

    comm->barrier();

    verbs->put( b1, 0, (comm->pid() + 1)%comm->nprocs(), b2, 0, sizeof(buf1));

    verbs->sync(true);
    EXPECT_EQ( "Hi", std::string(buf1) );
    EXPECT_EQ( "Hi", std::string(buf2) );
}


TEST_F( IBVerbsTests, get )
{

    char buf1[30] = "Hoi";
    char buf2[30] = "Vreemd";

    verbs->resizeMemreg( 2 );
    verbs->resizeMesgq( 1 );

    IBVerbs::SlotID b1 = verbs->regLocal( buf1, sizeof(buf1) );
    IBVerbs::SlotID b2 = verbs->regGlobal( buf2, sizeof(buf2) );

    comm->barrier();

    verbs->get( (comm->pid() + 1)%comm->nprocs(), b2, 0,
            b1, 0, sizeof(buf2));

    verbs->sync(true);
    EXPECT_EQ( "Vreemd", std::string(buf1) );
    EXPECT_EQ( "Vreemd", std::string(buf2) );
}


TEST_F( IBVerbsTests, putAllToAll )
{
    int nprocs = comm->nprocs();
    int pid = comm->pid();
    
    const int H = 2.5 * nprocs;

    std::vector< int > a(H);
    std::vector< int > b(H);

    for (int i = 0; i < H; ++i) {
        a[i] = i * nprocs + pid ;
        b[i] = nprocs*nprocs - ( i * nprocs + pid);
    }

    verbs->resizeMemreg( 2 );
    verbs->resizeMesgq( H );

    IBVerbs::SlotID a1 = verbs->regGlobal( a.data(), sizeof(int)*a.size());
    IBVerbs::SlotID b1 = verbs->regGlobal( b.data(), sizeof(int)*b.size());

    comm->barrier();

    for (int i = 0; i < H; ++i) {
        int dstPid = (pid + i ) % nprocs;
        verbs->put( a1, sizeof(int)*i,
                dstPid, b1, sizeof(int)*i, sizeof(int));
    }

    verbs->sync(true);

    for (int i = 0; i < H; ++i) {
        int srcPid = (nprocs + pid - (i%nprocs)) % nprocs;
        EXPECT_EQ( i*nprocs + pid, a[i] ) ;
        EXPECT_EQ( i*nprocs + srcPid, b[i] );
    }

}

TEST_F( IBVerbsTests, getAllToAll )
{
    int nprocs = comm->nprocs();
    int pid = comm->pid();

    const int H = 1000.3 * nprocs;

    std::vector< int > a(H);
    std::vector< int > b(H);

    for (int i = 0; i < H; ++i) {
        a[i] = i * nprocs + pid ;
        b[i] = nprocs*nprocs - ( i * nprocs + pid);
    }

    verbs->resizeMemreg( 2 );
    verbs->resizeMesgq( H );

    IBVerbs::SlotID a1 = verbs->regGlobal( a.data(), sizeof(int)*a.size());
    IBVerbs::SlotID b1 = verbs->regGlobal( b.data(), sizeof(int)*b.size());

    comm->barrier();

    for (int i = 0; i < H; ++i) {
        int srcPid = (pid + i) % nprocs;
        verbs->get( srcPid, a1, sizeof(int)*i,
                b1, sizeof(int)*i, sizeof(int));
    }

    verbs->sync(true);

    for (int i = 0; i < H; ++i) {
        int srcPid = (nprocs + pid + i ) % nprocs;
        EXPECT_EQ( i*nprocs + pid, a[i] ) ;
        EXPECT_EQ( i*nprocs + srcPid, b[i] );
    }

}


TEST_F( IBVerbsTests, putHuge )
{
    LOG(4, "Allocating mem1 ");
    std::vector< char > hugeMsg( std::numeric_limits<int>::max() * 1.5l );
    LOG(4, "Allocating mem2 ");
    std::vector< char > hugeBuf( hugeMsg.size() );

#if 0
    LOG(4, "Initializing mem2 ");
    for ( size_t i = 0; i < hugeMsg.size() ; ++i)
        hugeMsg[i] = char( i );
#endif

    verbs->resizeMemreg( 2 );
    verbs->resizeMesgq( 1 );

    IBVerbs::SlotID b1 = verbs->regLocal( hugeMsg.data(), hugeMsg.size() );
    IBVerbs::SlotID b2 = verbs->regGlobal( hugeBuf.data(), hugeBuf.size() );

    comm->barrier();

    verbs->put( b1, 0, (comm->pid() + 1)%comm->nprocs(), b2, 0, hugeMsg.size() );

    verbs->sync(true);

    EXPECT_EQ( hugeMsg, hugeBuf );
}

TEST_F( IBVerbsTests, getHuge )
{

    std::vector< char > hugeMsg( std::numeric_limits<int>::max() * 1.5 );
    std::vector< char > hugeBuf( hugeMsg.size() );

    for ( size_t i = 0; i < hugeMsg.size() ; ++i)
        hugeMsg[i] = char( i );

    verbs->resizeMemreg( 2 );
    verbs->resizeMesgq( 1 );

    IBVerbs::SlotID b1 = verbs->regLocal( hugeBuf.data(), hugeBuf.size() );
    IBVerbs::SlotID b2 = verbs->regGlobal( hugeMsg.data(), hugeMsg.size() );

    comm->barrier();

    verbs->get( (comm->pid() + 1)%comm->nprocs(), b2, 0, b1, 0, hugeMsg.size() );

    verbs->sync(true);

    EXPECT_EQ( hugeMsg, hugeBuf );
}

TEST_F( IBVerbsTests, manyPuts )
{

    const unsigned N = 100000;
    std::vector< unsigned char > buf1( N );
    std::vector< unsigned char > buf2( N );
    for (unsigned int i = 0 ; i < N; ++ i)
        buf1[i] = i + comm->pid() ;

    verbs->resizeMemreg( 2 );
    verbs->resizeMesgq( N );

    IBVerbs::SlotID b1 = verbs->regLocal( buf1.data(), buf1.size()  );
    IBVerbs::SlotID b2 = verbs->regGlobal( buf2.data(), buf1.size() );

    comm->barrier();

    for ( unsigned i = 0 ; i < N; ++i)
        verbs->put( b1, i, (comm->pid() + 1)%comm->nprocs(), b2, i, 1);

    verbs->sync(true);
    for ( unsigned i = 0 ; i < N; ++i) {
        unsigned char b2_exp = i + (comm->pid() + comm->nprocs() - 1)  % comm->nprocs();
        unsigned char b1_exp = i + comm->pid();
        EXPECT_EQ( b2_exp, buf2[i]);
        EXPECT_EQ( b1_exp, buf1[i] );
    }
}

