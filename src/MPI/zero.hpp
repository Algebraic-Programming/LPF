
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

#ifndef LPF_CORE_MPI_ZERO_HPP
#define LPF_CORE_MPI_ZERO_HPP

#include <atomic>
#include <limits>
#include <string>
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
        MemoryRegistration(
            char * addr, size_t size,
            uint32_t lkey, uint32_t rkey,
            int pid
        ) : _addr(addr), _size(size), _lkey(lkey), _rkey(rkey), _pid(pid)
        {}
        MemoryRegistration() :
            _addr(nullptr), _size(0),
            _lkey(0), _rkey(0), _pid(-1)
        {}
        size_t serialize(char ** buf);
        static MemoryRegistration * deserialize(char * buf);
};

class _LPFLIB_LOCAL Zero
{

public:

    typedef size_t SlotID;
    typedef uint32_t TagID;

    static constexpr TagID INVALID_TAG = std::numeric_limits<TagID>::max();

    struct Exception;

    struct SyncAttr {
        TagID tag;
        size_t expected_sent;
        size_t expected_rcvd;
    };

    explicit Zero( Communication & );
    ~Zero();

    void resizeMemreg( size_t size );
    void resizeMesgq( size_t size );
    void resizeTagreg( size_t size );

    SlotID regLocal( void * addr, size_t size );
    SlotID regGlobal( void * addr, size_t size );
    TagID regTag();

    void dereg( SlotID id );
    void deregTag( TagID id );

    size_t getMaxMsgSize() const {
        return m_maxMsgSize;
    }

    void put( SlotID srcSlot, size_t srcOffset,
              int dstPid, SlotID dstSlot, size_t dstOffset, size_t size );

    void get( int srcPid, SlotID srcSlot, size_t srcOffset,
              SlotID dstSlot, size_t dstOffset, size_t size );

    void flushSent();

    void flushReceived();

    void doRemoteProgress();

    void countingSyncPerSlot(const TagID tag, const size_t sent,
        const size_t recvd);

    /**
     * @syncPerTag only guarantees that all already scheduled sends (via put),
     * or receives (via get) associated with a slot are completed. It does
     * not guarantee that not scheduled operations will be scheduled (e.g.
     * no guarantee that a remote process will wait til data is put into its
     * memory, as it does schedule the operation (one-sided).
     */
    void syncPerTag(TagID tag);

    // Do the communication and synchronize
    // 'Reconnect' must be a globally replicated value
    void sync(bool reconnect, const struct SyncAttr * attr);

    void get_rcvd_msg_count(size_t &rcvd_msgs,
        const struct SyncAttr * attr) noexcept;
    void get_sent_msg_count(size_t &sent_msgs,
        const struct SyncAttr * attr) noexcept;

    void createNewSyncAttr(struct SyncAttr * * attr);

    inline void destroySyncAttr(struct SyncAttr * attr)
    {
        delete attr;
    }

    inline TagID getTag(const struct SyncAttr &attr) noexcept
    {
        return attr.tag;
    }

    inline void setTag(const TagID tag, struct SyncAttr &attr) noexcept
    {
        attr.tag = tag;
    }

    inline void setZCAttr(size_t sent, size_t rcvd, struct SyncAttr &attr)
        noexcept
    {
        attr.expected_sent = sent;
        attr.expected_rcvd = rcvd;
    }

    inline void getZCAttr(const struct SyncAttr &attr,
        size_t &sent, size_t &rcvd) noexcept
    {
        sent = attr.expected_sent;
        rcvd = attr.expected_rcvd;
    }

protected:
    Zero & operator=(const Zero & ); // assignment prohibited
    Zero( const Zero & ); // copying prohibited

    void stageQPs(size_t maxMsgs );
    void reconnectQPs();

    void doProgress();
    void tryIncrement(const Op op, const Phase phase, const TagID slot)
        noexcept;

    std::vector<ibv_wc_opcode> doLocalProgress(int& error);

    struct MemorySlot {
        shared_ptr< struct ibv_mr > mr;    // verbs structure
        std::vector< MemoryRegistration > glob; // array for global registrations
    };

    int    m_pid;    // local process ID
    int    m_nprocs; // number of processes
    int    m_ibPort; // local IB port to work with
    int    m_gidIdx;
    size_t m_maxRegSize;
    size_t m_maxMsgSize;
    size_t m_cqSize;
    size_t m_minNrMsgs;
    size_t m_maxSrs; // maximum number of sends requests per QP
    size_t m_postCount;
    size_t m_recvCount;
    size_t m_tag_capacity;

    shared_ptr< struct ibv_context > m_device;      // device handle
    shared_ptr< struct ibv_pd >      m_pd;          // protection domain
    shared_ptr< struct ibv_cq >      m_cq;          // complation queue
    shared_ptr< struct ibv_cq >      m_cqLocal;     // completion queue
    shared_ptr< struct ibv_cq >      m_cqRemote;    // completion queue
    shared_ptr< struct ibv_srq >     m_srq;         // shared receive queue
    shared_ptr< struct ibv_mr >      m_dummyMemReg; // registration of dummy
                                                    // buffer
    std::atomic_size_t m_numMsgs;
    std::atomic_size_t m_recvTotalInitMsgCount;
    std::atomic_size_t m_sentMsgs;
    std::atomic_size_t m_recvdMsgs;

    uint16_t     m_lid;     // LID of the IB port

    Communication & m_comm;

    std::string  m_devName; // IB device name

    ibv_mtu      m_mtu;

    struct ibv_device_attr m_deviceAttr;

    std::vector<TagID>  m_free_tags;
    std::vector<size_t> m_recvInitMsgCount;
    std::vector<size_t> m_getInitMsgCount;
    std::vector<size_t> m_sendInitMsgCount;

    // Disconnected queue pairs
    std::vector< shared_ptr< struct ibv_qp > > m_stagedQps;

    // Connected queue pairs
    std::vector< shared_ptr< struct ibv_qp > > m_connectedQps;

    std::vector< struct ibv_send_wr > m_srs;          // array of send requests
    std::vector< size_t >             m_srsHeads;     // head of send queue per
                                                      // peer
    std::vector< size_t >             m_nMsgsPerPeer; // number of messages per
                                                      // peer
    std::vector< pid_t >              m_peerList;

    std::vector< struct ibv_sge > m_sges;        // array of scatter/gather
                                                 // entries
    std::vector< struct ibv_wc >  m_wcs;         // array of work completions
    std::vector< char >           m_dummyBuffer; // dummy receive buffer

    std::vector<size_t> rcvdMsgCount;
    std::vector<size_t> sentMsgCount;
    std::vector<size_t> getMsgCount;
    std::vector<bool>   tagActive;

    SparseSet< pid_t > m_activePeers;

    CombinedMemoryRegister< MemorySlot > m_memreg;

};


} }


#endif
