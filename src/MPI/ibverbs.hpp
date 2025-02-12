
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
#if __cplusplus >= 201103L    
  #include <memory>
#else
  #include <tr1/memory>
#endif

#include <infiniband/verbs.h>


#include "linkage.hpp"
#include "sparseset.hpp"
#include "memreg.hpp"

typedef enum Op {
    SEND,
    RECV,
    GET
} Op;

typedef enum Phase {
    INIT,
    PRE,
    POST
} Phase;

namespace lpf {
    
    class Communication;
    
    namespace mpi {

#if __cplusplus >= 201103L    
using std::shared_ptr;
#else
using std::tr1::shared_ptr;
#endif

class MemoryRegistration {
    public:
        char *   _addr;
        size_t   _size;
        uint32_t _lkey;
        uint32_t _rkey;
        int _pid;
        MemoryRegistration(char * addr, size_t size, uint32_t lkey, uint32_t rkey, int pid) : _addr(addr),
        _size(size), _lkey(lkey), _rkey(rkey), _pid(pid)
        { }
        MemoryRegistration() : _addr(nullptr), _size(0), _lkey(0), _rkey(0), _pid(-1) {}
        size_t serialize(char ** buf);
        static MemoryRegistration * deserialize(char * buf);

};


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

    size_t getMaxMsgSize() const {
        return m_maxMsgSize;
    }

    void blockingCompareAndSwap(SlotID srSlot, size_t srcOffset, int dstPid, SlotID dstSlot, size_t dstOffset, size_t size, uint64_t compare_add, uint64_t swap);

    void put( SlotID srcSlot, size_t srcOffset, 
              int dstPid, SlotID dstSlot, size_t dstOffset, size_t size );

    void get( int srcPid, SlotID srcSlot, size_t srcOffset, 
              SlotID dstSlot, size_t dstOffset, size_t size );

    void flushSent();

    void flushReceived();

    void doRemoteProgress();

    void countingSyncPerSlot(SlotID tag, size_t sent, size_t recvd);
    /**
     * @syncPerSlot only guarantees that all already scheduled sends (via put), 
     * or receives (via get) associated with a slot are completed. It does 
     * not guarantee that not scheduled operations will be scheduled (e.g.
     * no guarantee that a remote process will wait til data is put into its 
     * memory, as it does schedule the operation (one-sided).
     */
    void syncPerSlot(SlotID slot);

    // Do the communication and synchronize
    // 'Reconnect' must be a globally replicated value
    void sync( bool reconnect);

    void get_rcvd_msg_count(size_t * rcvd_msgs);
    void get_sent_msg_count(size_t * sent_msgs);
    void get_rcvd_msg_count_per_slot(size_t * rcvd_msgs, SlotID slot);
    void get_sent_msg_count_per_slot(size_t * sent_msgs, SlotID slot);

protected:
    IBVerbs & operator=(const IBVerbs & ); // assignment prohibited
    IBVerbs( const IBVerbs & ); // copying prohibited

    void stageQPs(size_t maxMsgs ); 
    void reconnectQPs(); 
    void tryLock(SlotID id, int dstPid);
    void tryUnlock(SlotID id, int dstPid);

    std::vector<ibv_wc_opcode> wait_completion(int& error);
    void doProgress();
    void tryIncrement(Op op, Phase phase, SlotID slot);

    struct MemorySlot {
        shared_ptr< struct ibv_mr > mr;    // verbs structure
        std::vector< MemoryRegistration > glob; // array for global registrations
    };


    Communication & m_comm;
    int          m_pid; // local process ID
    int          m_nprocs; // number of processes
    std::atomic_size_t m_numMsgs;
    std::atomic_size_t m_recvTotalInitMsgCount;
    std::atomic_size_t m_sentMsgs;
    std::atomic_size_t m_recvdMsgs;
    std::vector<size_t> m_recvInitMsgCount;
    std::vector<size_t> m_getInitMsgCount;
    std::vector<size_t> m_sendInitMsgCount;

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

    shared_ptr< struct ibv_context > m_device; // device handle
    shared_ptr< struct ibv_pd >      m_pd;     // protection domain
    shared_ptr< struct ibv_cq >      m_cq;     // complation queue
   	shared_ptr< struct ibv_cq >		 m_cqLocal;	// completion queue
	shared_ptr< struct ibv_cq >		 m_cqRemote;	// completion queue
    shared_ptr< struct ibv_srq >		 m_srq;	 	// shared receive queue

    // Disconnected queue pairs
    std::vector< shared_ptr<struct ibv_qp> > m_stagedQps; 

    // Connected queue pairs
    std::vector< shared_ptr<struct ibv_qp> > m_connectedQps; 



    std::vector< struct ibv_send_wr > m_srs; // array of send requests
    std::vector< size_t >        m_srsHeads; // head of send queue per peer
    std::vector< size_t >        m_nMsgsPerPeer; // number of messages per peer
    SparseSet< pid_t >           m_activePeers; // 
    std::vector< pid_t >         m_peerList;

    std::vector< struct ibv_sge > m_sges; // array of scatter/gather entries
    std::vector< struct ibv_wc > m_wcs; // array of work completions

    CombinedMemoryRegister< MemorySlot > m_memreg;


    shared_ptr< struct ibv_mr > m_dummyMemReg; // registration of dummy buffer
    std::vector< char > m_dummyBuffer; // dummy receive buffer
                                       //
    std::vector<size_t> rcvdMsgCount;
    std::vector<size_t> sentMsgCount;
    std::vector<size_t> getMsgCount;
    std::vector<bool> slotActive;
    size_t m_postCount;
    size_t m_recvCount;
};



} }


#endif
