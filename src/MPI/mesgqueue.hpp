
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

#ifndef LPF_CORE_MESGQUEUE_HPP
#define LPF_CORE_MESGQUEUE_HPP

#include <vector>
#include <iosfwd>
#include "memorytable.hpp"
#include "types.hpp"
#include "vall2all.hpp"
#include "messagesort.hpp"
#include "mpilib.hpp"
#include "linkage.hpp"

#if __cplusplus >= 201103L
#include <memory>
#else
#include <tr1/memory>
#endif

#if defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
#include "ibverbsNoc.hpp"
#endif

//only for HiCR
typedef size_t SlotID;

namespace lpf {

class _LPFLIB_LOCAL MessageQueue
{

public:
    explicit MessageQueue( Communication & comm );

    err_t resizeMemreg( size_t nRegs );
    err_t resizeMesgQueue( size_t nMsgs );


    memslot_t addLocalReg( void * mem, std::size_t size );
    memslot_t addGlobalReg( void * mem, std::size_t size );
    memslot_t addNocReg( void * mem, std::size_t size );
    void      removeReg( memslot_t slot );

    void get( pid_t srcPid, memslot_t srcSlot, size_t srcOffset,
            memslot_t dstSlot, size_t dstOffset, size_t size );

    void put( memslot_t srcSlot, size_t srcOffset,
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size );


    // returns how many processes have entered in an aborted state
    int sync( bool abort );

//only for HiCR
//#ifdef 
    void lockSlot( memslot_t srcSlot, size_t srcOffset,
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size );

    void unlockSlot( memslot_t srcSlot, size_t srcOffset,
		    pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size );

    void getRcvdMsgCountPerSlot(size_t * msgs, SlotID slot);

    void getRcvdMsgCount(size_t * msgs);

    void getSentMsgCountPerSlot(size_t * msgs, SlotID slot);

    void flushSent();

    void flushReceived();

    int countingSyncPerSlot(SlotID slot, size_t expected_sent, size_t expected_rcvd);

    int syncPerSlot(SlotID slot);
// end only for HiCR
//#endif

private:
    enum Msgs { BufPut , 
        BufGet, BufGetReply,
        HpPut, HpGet , HpBodyReply ,
        HpEdges, HpEdgesReply };

    enum Props { MsgId, Tag,
        SrcPid, DstPid,
        SrcOffset, DstOffset, BufOffset,
        SrcSlot, DstSlot, Size,
        RoundedDstOffset, RoundedSize, 
        Payload, Head, Tail};

    struct Edge {
        MessageSort::MsgId id;
#ifdef LPF_CORE_MPI_USES_mpimsg
        unsigned tag;
#endif
        bool canWriteHead, canWriteTail;
        pid_t srcPid, dstPid;
        memslot_t srcSlot, dstSlot;
        size_t srcOffset;
        size_t dstOffset;
        size_t size;
        size_t roundedDstOffset, roundedSize;
        size_t bufOffset;
    };

    struct Body {
        MessageSort::MsgId id;
#ifdef LPF_CORE_MPI_USES_mpimsg
        unsigned tag;
#endif
        pid_t srcPid, dstPid;
        memslot_t srcSlot, dstSlot;
        size_t    srcOffset, dstOffset;
        size_t    size;
        size_t roundedDstOffset, roundedSize;
    };

    static size_t largestHeader( lpf_pid_t nprocs, size_t memRange, size_t maxRegs, size_t maxMsgs);


    typedef mpi::VirtualAllToAll Queue;
    static Queue * newQueue( pid_t pid, pid_t nprocs );

    const pid_t m_pid, m_nprocs;
    const size_t m_memRange;
    const size_t m_tinyMsgSize;
    const size_t m_smallMsgSize;
    std::vector< int > m_vote;
#if __cplusplus >= 201103L
    std::shared_ptr<Queue> m_firstQueue, m_secondQueue;
#else
    std::tr1::shared_ptr<Queue> m_firstQueue, m_secondQueue;
#endif
    size_t m_maxNMsgs;
    size_t m_nextMemRegSize;
    bool m_resized;
    MessageSort m_msgsort;
    std::vector< Body > m_bodyRequests;
    std::vector< Edge > m_edgeRecv;
    std::vector< Edge > m_edgeSend;
    std::vector< char > m_edgeBuffer;
#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
    memslot_t m_edgeBufferSlot;
#endif
    std::vector< Body > m_bodySends;
    std::vector< Body > m_bodyRecvs;
    mpi::Comm m_comm;
    std::vector< char > m_tinyMsgBuf;

protected:
#if defined LPF_CORE_MPI_USES_ibverbs  || defined LPF_CORE_MPI_USES_zero
    mpi::IBVerbs m_ibverbs;
#endif
    MemoryTable m_memreg;
};


}  // namespace lpf


#endif
