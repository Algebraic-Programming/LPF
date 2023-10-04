
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

#ifndef LPF_CORE_MPI_IBVERBS_HPP
#define LPF_CORE_MPI_IBVERBS_HPP

#include <string>
#include <atomic>
#include <vector>
#include <map>
#include <memory>
#include <thread>
//#if __cplusplus >= 201103L    
//  #include <memory>
//#else
//  #include <tr1/memory>
//#endif

#include <infiniband/verbs.h>


#include "linkage.hpp"
#include "sparseset.hpp"
#include "memreg.hpp"

namespace lpf {
    
    class Communication;
    
    namespace mpi {

#if __cplusplus >= 201103L    
using std::shared_ptr;
#else
using std::tr1::shared_ptr;
#endif

class _LPFLIB_LOCAL IBVerbs 
{
public:
    struct Exception;

    typedef size_t SlotID;

    explicit IBVerbs( Communication & );
    ~IBVerbs();

    void resizeMemreg( size_t size );
    void resizeMesgq( size_t size );
    
    SlotID regLocal( void * addr, size_t size );
    SlotID regGlobal( void * addr, size_t size );
    void dereg( SlotID id );

    void put( SlotID srcSlot, size_t srcOffset, 
              int dstPid, SlotID dstSlot, size_t dstOffset, size_t size, SlotID firstDstSlot);

    void get( int srcPid, SlotID srcSlot, size_t srcOffset, 
              SlotID dstSlot, size_t dstOffset, size_t size );


    void doRemoteProgress();

    // Do the communication and synchronize
    // 'Reconnect' must be a globally replicated value
    void sync( bool reconnect);

    void get_rcvd_msg_count(size_t * rcvd_msgs, SlotID slot);
private:
    IBVerbs & operator=(const IBVerbs & ); // assignment prohibited
    IBVerbs( const IBVerbs & ); // copying prohibited

    void stageQPs(size_t maxMsgs ); 
    void reconnectQPs(); 

    void post_sends();
    void wait_completion(int& error);
    void doProgress();

    struct MemoryRegistration {
        void *   addr;
        size_t   size;
        uint32_t lkey;
        uint32_t rkey;
    };

    struct MemorySlot {
        shared_ptr< struct ibv_mr > mr;    // verbs structure
        std::vector< MemoryRegistration > glob; // array for global registrations
    };

    struct UserContext {
        size_t lkey;
    };

    int          m_pid; // local process ID
    int          m_nprocs; // number of processes
    std::atomic_size_t m_numMsgs;
    std::atomic_size_t m_sentMsgs;

    std::string  m_devName; // IB device name
    int          m_ibPort;  // local IB port to work with
    int          m_gidIdx; 
    uint16_t     m_lid;     // LID of the IB port
    ibv_mtu      m_mtu;   
    struct ibv_device_attr m_deviceAttr;
    size_t       m_maxRegSize;
    size_t       m_maxMsgSize; 
    size_t		m_cqSize;
    size_t       m_minNrMsgs;
    size_t       m_maxSrs; // maximum number of sends requests per QP  
    size_t m_postCount;
    size_t m_recvCount;
    std::atomic_int m_stopProgress;

    int *m_recvCounts;
    shared_ptr< struct ibv_context > m_device; // device handle
    shared_ptr< struct ibv_pd >      m_pd;     // protection domain
   	shared_ptr< struct ibv_cq >		 m_cqLocal;	// completion queue
	shared_ptr< struct ibv_cq >		 m_cqRemote;	// completion queue
    shared_ptr< struct ibv_srq >		 m_srq;	 	// shared receive queue

    // Disconnected queue pairs
    std::vector< shared_ptr< struct ibv_qp > > m_stagedQps; 

    // Connected queue pairs
    std::vector< shared_ptr< struct ibv_qp > > m_connectedQps; 


    std::vector< struct ibv_send_wr > m_srs; // array of send requests
    std::vector< size_t >        m_srsHeads; // head of send queue per peer
    std::vector< size_t >        m_nMsgsPerPeer; // number of messages per peer
    SparseSet< pid_t >           m_activePeers; // 
    std::vector< pid_t >         m_peerList;
    shared_ptr<std::thread> progressThread;
    std::map<SlotID, std::atomic_size_t> rcvdMsgCount;

    std::vector< struct ibv_sge > m_sges; // array of scatter/gather entries
    //std::vector< struct ibv_wc > m_wcs; // array of work completions

    CombinedMemoryRegister< MemorySlot > m_memreg;


    shared_ptr< struct ibv_mr > m_dummyMemReg; // registration of dummy buffer
    std::vector< char > m_dummyBuffer; // dummy receive buffer

    Communication & m_comm;
};



} }


#endif
