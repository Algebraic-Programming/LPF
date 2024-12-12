
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

#include "mesgqueue.hpp"
#include "ibverbs.hpp"
#include "mpilib.hpp"
#include "log.hpp"
#include "assert.hpp"
#include "ipcmesg.hpp"
#include "config.hpp"
#include "dall2all.hpp"
#include "spall2all.hpp"

#ifdef MPI_HAS_IBARRIER
#include "hall2all.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace lpf {


size_t MessageQueue :: largestHeader( lpf_pid_t nprocs, size_t memRange, size_t maxRegs, size_t maxMsgs)
{
    using std::ceil;
    using std::max;
    size_t size = static_cast<size_t>(
           2* ceil( max( 1.0, log2(nprocs) / 7.0 ))    // srcPid, dstPid
        +  2* ceil( max( 1.0, log2(1+maxRegs) / 7.0 )) // srcSlot + dstSlot
        +  6* ceil( max( 1.0, log2(1+memRange) / 7.0 )) // srcOffset + dstOffset + size
                                        // + roundedDstOffset + roundedSize + bufOffset
        +  2* ceil( max( 1.0, log2(1+maxMsgs) / 7.0 )) // msgId + tag
        +  2                       // canWriteHead + canWriteTail
        );

    LOG( 3, "The largest message header in MPI engine is " << size << " bytes, because nprocs = "
            << nprocs << ", memory range = "
            << memRange << ", max registers = " << maxRegs << " and max messages = " << maxMsgs );

    ASSERT( size >= 14 );
    return size
#ifndef NDEBUG
       + 14 * 100 /* in Debug mode, type names of properties are
                    are also transported */
#endif
       ;
}


MessageQueue :: Queue *
    MessageQueue :: newQueue( pid_t pid, pid_t nprocs )
{
  Config::A2AMode mode = Config::instance().getA2AMode() ;
  if ( mode == Config::A2A_DENSE )
      return new mpi::DenseAllToAll( pid, nprocs, 2 );
  else if (mode == Config::A2A_HOEFLER) {
#ifdef MPI_HAS_IBARRIER
      return new mpi::HoeflerAllToAll( pid, nprocs, 2 );
#else
      LOG(1, "Hoefler all-to-all is not available for this MPI implementation. Using sparse all-to-all instead");
#endif
  }

  return new mpi::SparseAllToAll( pid, nprocs, 2 );
}

MessageQueue :: MessageQueue( Communication & comm )
    : m_pid(comm.pid())
    , m_nprocs(comm.nprocs())
    , m_memRange( comm.allreduceMax( Config::instance().getLocalRamSize()) )
    , m_tinyMsgSize( Config::instance().getTinyMsgSize().sizeForMPI )
    , m_smallMsgSize( 2 * (1 << Config::instance().getMpiMsgSortGrainSizePower()) )
    , m_vote( 2 )
    , m_firstQueue( newQueue(m_pid, m_nprocs) )
    , m_secondQueue( newQueue(m_pid, m_nprocs) )
    , m_maxNMsgs( 0 )
    , m_nextMemRegSize( 0 )
    , m_resized( false )
    , m_msgsort()
    , m_bodyRequests()
    , m_edgeRecv()
    , m_edgeSend()
    , m_edgeBuffer()
#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
    , m_edgeBufferSlot( m_memreg.invalidSlot() )
#endif
    , m_bodySends()
    , m_bodyRecvs()
    , m_comm( dynamic_cast<mpi::Comm &>(comm) )
    , m_tinyMsgBuf( m_tinyMsgSize + largestHeader(m_nprocs, m_memRange, 0, 0))
#if defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
    , m_ibverbs(m_comm)
    , m_memreg( m_comm, m_ibverbs )
#else
    , m_memreg( m_comm )
#endif
{
    m_memreg.reserve(1); // reserve slot for edgeBuffer
}

err_t MessageQueue :: resizeMesgQueue( size_t nMsgs )
{
    const size_t maxNMsgs = std::max( m_maxNMsgs, nMsgs);
    const size_t nRegs = m_memreg.capacity();
    const size_t maxHdrSize =
        largestHeader( m_nprocs, m_memRange, nRegs, maxNMsgs );
    const size_t maxMsgSize = m_tinyMsgSize + maxHdrSize;

    if ( nMsgs > m_firstQueue->max_size(maxMsgSize) / 2
            || nMsgs > std::numeric_limits<size_t>::max() / 6
            || nMsgs > std::numeric_limits<size_t>::max() / m_smallMsgSize )
    {
        LOG( 2, "Requested message queue size exceeds theoretical capacity");
        return LPF_ERR_OUT_OF_MEMORY;
    }

    ASSERT( nMsgs <= m_firstQueue->max_size(maxMsgSize) / 2 );

    size_t mult = 2;
    // one factor two is required because write conflict resolution can
    // fragment messages

    // The sparse all-to-all needs a bit more buffer memory
    // Compute using Chernoff bounds the maximum congestion
    if ( dynamic_cast<mpi::SparseAllToAll*>(&*m_firstQueue))
    {
        double max_h = (double) mult*nMsgs;
        double epsilon = 1e-20; // acceptable probability of failure
        if ( exp( -0.3333 * max_h ) < epsilon )
            mult *= 2;
        else
            mult *= size_t(3 - std::log( epsilon ) / max_h );
    }

    if ( (double) nMsgs > (double) std::numeric_limits<size_t>::max() / mult)
    {
        LOG( 2, "Requested message queue size exceeds theoretical capacity"
                " because multiplication factor is too high: " << mult );
        return LPF_ERR_OUT_OF_MEMORY;
    }

    const size_t newCap = mult * nMsgs;

    LOG(3, "Reserving " << mult << "x the memory in Sparse-all-to-all buffer: "
            << newCap << " messages x ( "
            << maxHdrSize << " + " << m_tinyMsgSize << ") bytes ." );

    m_resized = true;
    try
    {
        m_firstQueue->reserve( newCap,  maxMsgSize );
        m_secondQueue->reserve( newCap, maxMsgSize );

        if (m_bodyRequests.size() < nMsgs ) {
            m_bodyRequests.resize( nMsgs ); // need only exactly nMsgs because each entry
            m_edgeRecv.resize( nMsgs ); // matches exactly with one lpf_put or lpf_get on the
                                        // the destination.
        }
        m_edgeSend.reserve( nMsgs );
        m_edgeBuffer.reserve( m_smallMsgSize * nMsgs );
        m_bodySends.reserve( 2* nMsgs ); // one factor two is required because
        m_bodyRecvs.reserve( 2* nMsgs ); // messages can get fragmented
#ifdef LPF_CORE_MPI_USES_mpimsg
        m_comm.reserveMsgs( 6* nMsgs ); //another factor three stems from sending edges separately .
#endif
#if defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
        m_ibverbs.resizeMesgq( 6*nMsgs);
#endif

        m_maxNMsgs = maxNMsgs;
    }
    catch (std::bad_alloc & )
    {
        LOG(2, "Insufficient memory for increasing message queue size to "
                << mult << "x " << nMsgs << " = " << newCap <<
                " messages. This would have taken up " << newCap << " x ( "
            << maxHdrSize << " (meta-data)  + " << m_tinyMsgSize << " (tiny msg payload) ) bytes"
           << " in the meta-data exchange buffer alone." );
        return LPF_ERR_OUT_OF_MEMORY;
    }
    catch (std::length_error &)
    {
        LOG(2, "Insufficient memory for increasing message queue size to "
                << mult << "x " << nMsgs << " = " << newCap <<
                " messages. This would have taken up " << newCap << " x ( "
            << maxHdrSize << " (meta-data)  + " << m_tinyMsgSize << " (tiny msg payload) ) bytes"
           << " in the meta-data exchange buffer alone." );
        return LPF_ERR_OUT_OF_MEMORY;
    }
    return LPF_SUCCESS;
}

err_t MessageQueue :: resizeMemreg( size_t nRegs )
{
    if ( nRegs > std::numeric_limits<size_t>::max() - 1) {
        LOG( 2, "Overflow when computing number of memory slots to reserve");
        return LPF_ERR_OUT_OF_MEMORY;
    }

    try
    {
        if ( m_memreg.capacity() < nRegs + 1 )
        {
            m_memreg.reserve( nRegs + 1 );
            m_msgsort.setSlotRange( m_memreg.range() );
        }
    }
    catch( std::bad_alloc & )
    {
        LOG( 2, "Insufficient memory for increasing number of memory slots");
        return LPF_ERR_OUT_OF_MEMORY;
    }
    catch( std::length_error & )
    {
        LOG( 2, "Insufficient memory for increasing number of memory slots");
        return LPF_ERR_OUT_OF_MEMORY;
    }

    if ( LPF_SUCCESS != resizeMesgQueue(m_maxNMsgs) ) {
        LOG( 2, "Insufficient memory for increasing number of memory slots"
                 << ", because message meta-data would grow too much" );
        return LPF_ERR_OUT_OF_MEMORY;
    }

    m_nextMemRegSize = nRegs + 1;

    return LPF_SUCCESS;
}


memslot_t MessageQueue :: addNocReg( void * mem, std::size_t size)
{
    memslot_t slot = m_memreg.addNoc( mem, size );
    ASSERT(slot != LPF_INVALID_MEMSLOT);
    if (size > 0)
        m_msgsort.addRegister( slot, static_cast<char *>( mem ), size);
    return slot;
}

err_t MessageQueue :: serializeSlot(memslot_t slot, char ** mem, std::size_t * size)
{
    ASSERT(slot != LPF_INVALID_MEMSLOT);
    ASSERT(mem != nullptr);
    ASSERT(size != nullptr);
#ifdef LPF_CORE_MPI_USES_zero
    auto mr = m_ibverbs.getMR(m_memreg.getVerbID(slot), m_pid);
    *size = mr.serialize(mem);
    return LPF_SUCCESS;
#else
    LOG( 3, "Error: serialize slot is only implemented for zero engine at the moment.");
    return LPF_ERR_FATAL;
#endif

}

err_t MessageQueue :: deserializeSlot(char * mem, memslot_t slot)
{
    ASSERT(mem != nullptr);
    ASSERT(slot != LPF_INVALID_MEMSLOT);
#ifdef LPF_CORE_MPI_USES_zero
    auto mr = mpi::MemoryRegistration::deserialize(mem);
    m_ibverbs.setMR(m_memreg.getVerbID(slot), mr->_pid, *mr);
    return LPF_SUCCESS;
#else
    LOG( 3, "Error: deserialize slot is only implemented for zero engine at the moment.");
    return LPF_ERR_FATAL;
#endif

}


memslot_t MessageQueue :: addLocalReg( void * mem, std::size_t size)
{
    memslot_t slot = m_memreg.addLocal( mem, size );
    if (size > 0)
        m_msgsort.addRegister( slot, static_cast<char *>( mem ), size);
    return slot;
}

memslot_t MessageQueue :: addGlobalReg( void * mem, std::size_t size )
{
    memslot_t slot = m_memreg.addGlobal( mem, size );
    if (size > 0)
        m_msgsort.addRegister( slot, static_cast<char *>(mem), size);
    return slot;
}

void MessageQueue :: removeReg( memslot_t slot )
{
    if (m_memreg.getSize( slot ) > 0)
        m_msgsort.delRegister(slot);

    m_memreg.remove( slot );
}

void MessageQueue :: get( pid_t srcPid, memslot_t srcSlot, size_t srcOffset,
        memslot_t dstSlot, size_t dstOffset, size_t size )
{
#ifdef LPF_CORE_MPI_USES_zero
    m_ibverbs.get(srcPid,
            m_memreg.getVerbID( srcSlot),
            srcOffset,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size );
#else
    if (size > 0)
    {
        ASSERT( ! m_memreg.isLocalSlot( srcSlot ) );
        void * address = m_memreg.getAddress( dstSlot, dstOffset );
        if ( srcPid == static_cast<pid_t>(m_pid) )
        {
            std::memcpy( address, m_memreg.getAddress( srcSlot, srcOffset), size);
        }
        else
        {
            using mpi::ipc::newMsg;

            if (size <= m_tinyMsgSize )
            {
                // send immediately the request to the source
                newMsg( BufGet, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() )
                    .write( DstPid ,  m_pid )
                    .write( SrcSlot, srcSlot)
                    .write( DstSlot, dstSlot)
                    .write( SrcOffset, srcOffset )
                    .write( DstOffset, dstOffset )
                    .write( Size, size )
                    .send( *m_firstQueue, srcPid );
            }
            else
            {
                // send the request to the destination process (this process)
                // for write conflict resolution
                newMsg( HpGet, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() )
                    .write( SrcPid, srcPid )
                    .write( DstPid, m_pid )
                    .write( SrcSlot, srcSlot )
                    .write( DstSlot, dstSlot )
                    .write( SrcOffset, srcOffset )
                    .write( DstOffset, dstOffset )
                    .write( Size, size )
                    . send( *m_firstQueue, m_pid );
            }
        }
    }
#endif
}

void MessageQueue :: lockSlot( memslot_t srcSlot, size_t srcOffset,
        pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size )
{
    ASSERT(srcSlot != LPF_INVALID_MEMSLOT);
    ASSERT(dstSlot != LPF_INVALID_MEMSLOT);
    (void) srcOffset;
    (void) dstOffset;
    (void) dstPid;
    (void) size;
#ifdef LPF_CORE_MPI_USES_zero
m_ibverbs.blockingCompareAndSwap(m_memreg.getVerbID(srcSlot), srcOffset, dstPid, m_memreg.getVerbID(dstSlot), dstOffset, size, 0ULL, 1ULL);
#endif
}

void MessageQueue :: unlockSlot( memslot_t srcSlot, size_t srcOffset,
        pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size )
{
    ASSERT(srcSlot != LPF_INVALID_MEMSLOT);
    ASSERT(dstSlot != LPF_INVALID_MEMSLOT);
    (void) srcOffset;
    (void) dstOffset;
    (void) dstPid;
    (void) size;
#ifdef LPF_CORE_MPI_USES_zero
m_ibverbs.blockingCompareAndSwap(m_memreg.getVerbID(srcSlot), srcOffset, dstPid, m_memreg.getVerbID(dstSlot), dstOffset, size, 1ULL, 0ULL);
#endif
}

void MessageQueue :: put( memslot_t srcSlot, size_t srcOffset,
        pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size )
{
#ifdef LPF_CORE_MPI_USES_zero
    m_ibverbs.put( m_memreg.getVerbID( srcSlot),
            srcOffset,
            dstPid,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size);
#else
    if (size > 0)
    {
        ASSERT( ! m_memreg.isLocalSlot( dstSlot ) );
        void * address = m_memreg.getAddress( srcSlot, srcOffset );
        if ( dstPid == static_cast<pid_t>(m_pid) )
        {
            std::memcpy( m_memreg.getAddress( dstSlot, dstOffset), address, size);
        }
        else
        {
            using mpi::ipc::newMsg;
            if (size <= m_tinyMsgSize )
            {
                newMsg( BufPut, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() )
                    .write( DstSlot, dstSlot )
                    .write( DstOffset, dstOffset )
                    .write( Payload, address, size )
                    . send( *m_firstQueue, dstPid );
            }
            else
            {
                newMsg( HpPut, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() )
                    .write( SrcPid, m_pid )
                    .write( DstPid, dstPid )
                    .write( SrcSlot, srcSlot )
                    .write( DstSlot, dstSlot )
                    .write( SrcOffset, srcOffset )
                    .write( DstOffset, dstOffset )
                    .write( Size, size )
                    .send( *m_firstQueue, dstPid );
            }
        }
    }
#endif

}

int MessageQueue :: sync( bool abort )
{
#ifdef LPF_CORE_MPI_USES_zero
    // if not, deal with normal sync
    (void) abort;
    m_memreg.sync();
	m_ibverbs.sync(m_resized);
    m_resized = false;
#else

    LOG(4, "mpi :: MessageQueue :: sync( abort " << (abort?"true":"false")
            << " )");
    using mpi::ipc::newMsg;
    using mpi::ipc::recvMsg;

    // 1. communicate all requests to their destination and also
    // communicate the buffered gets to the source
    const int trials = 5;
    bool randomize = false;
    m_vote[0] = abort?1:0;
    m_vote[1] = m_resized?1:0;
    LOG(4, "Executing 1st meta-data exchange");
    if ( m_firstQueue->exchange(m_comm, randomize, m_vote.data(), trials) )
    {
        LOG(2, "All " << trials << " sparse all-to-all attempts have failed");
        throw std::runtime_error("All sparse all-to-all attempts have failed");
    }
    if ( m_vote[0] != 0 ) {
        LOG(2, "Abort detected by sparse all-to-all");
        return m_vote[0];
    }

    m_resized = (m_vote[1] > 0);

    // Synchronize the memory registrations
#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs
    if (m_resized) {
        if (m_edgeBufferSlot != m_memreg.invalidSlot())
        {
            m_memreg.remove( m_edgeBufferSlot );
            m_edgeBufferSlot = m_memreg.invalidSlot();
        }
        ASSERT( m_edgeBufferSlot == m_memreg.invalidSlot() );

        LOG(4, "Registering edge buffer slot of size "
                << m_edgeBuffer.capacity() );

        m_edgeBufferSlot
           = m_memreg.addGlobal(m_edgeBuffer.data(), m_edgeBuffer.capacity());
    }
#endif

    LOG(4, "Syncing memory table" );
    m_memreg.sync();

    // shrink memory register if necessary
    ASSERT( m_nextMemRegSize <= m_memreg.capacity() );
    if ( m_memreg.capacity() > m_nextMemRegSize )
    {
        LOG(4, "Reducing size of memory table ");
        m_memreg.reserve( m_nextMemRegSize );
    }


    LOG(4, "Processing message meta-data" );

#ifdef LPF_CORE_MPI_USES_mpimsg
    int tagger = 0;
#endif
    MessageSort :: MsgId newMsgId = 0;

    // 2. Schedule unbuffered comm for write conflict resolution,
    //    and process buffered communication
    while ( !m_firstQueue->empty() )
    {
        mpi::IPCMesg<Msgs> msg = recvMsg<Msgs>( *m_firstQueue, m_tinyMsgBuf.data(), m_tinyMsgBuf.size());

        switch ( msg.type() )
        {
           case BufPut: {
               /* execute them now so, we don't have to think about them anymore */
                memslot_t dstSlot;
                size_t dstOffset;
                msg.read( DstSlot, dstSlot)
                   .read( DstOffset, dstOffset );

                void * addr = m_memreg.getAddress( dstSlot, dstOffset);

                msg.read( Payload, addr, msg.bytesLeft() );
                /* that's a relief :-) */
                break;
           }

           case BufGet: {
               /* process the buffered get now, and put it in the second queue */
                memslot_t srcSlot, dstSlot;
                pid_t dstPid;
                size_t srcOffset, dstOffset;
                size_t size;

                msg .read( DstPid,  dstPid )
                    .read( SrcSlot, srcSlot)
                    .read( DstSlot, dstSlot)
                    .read( SrcOffset, srcOffset )
                    .read( DstOffset, dstOffset )
                    .read( Size, size );

                ASSERT( msg.bytesLeft() == 0 );

                void * addr = m_memreg.getAddress(srcSlot, srcOffset);

                newMsg( BufGetReply, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() )
                    .write( DstSlot, dstSlot )
                    .write( DstOffset, dstOffset )
                    .write( Payload, addr, size )
                    . send( *m_secondQueue, dstPid );
                break;
            }

            case HpGet:
            case HpPut: {
                ASSERT( newMsgId < m_bodyRequests.size() );
                ASSERT( newMsgId < m_edgeRecv.size() );
                MessageSort :: MsgId id = newMsgId++; /* give it a unique ID */

                /* store the edges of a put in a separate queue */
                pid_t srcPid, dstPid;
                memslot_t srcSlot, dstSlot;
                size_t srcOffset, dstOffset;
                size_t size;
                msg .read( SrcPid, srcPid )
                    .read( DstPid, dstPid )
                    .read( SrcSlot, srcSlot )
                    .read( DstSlot, dstSlot )
                    .read( SrcOffset, srcOffset )
                    .read( DstOffset, dstOffset )
                    .read( Size, size );

                Body body;
                body.id = id;
#ifdef LPF_CORE_MPI_USES_mpimsg
                body.tag = -1;
#endif
                body.srcPid = srcPid;
                body.dstPid = dstPid;
                body.srcSlot = srcSlot;
                body.dstSlot = dstSlot;
                body.srcOffset = srcOffset;
                body.dstOffset = dstOffset;
                body.roundedDstOffset = dstOffset;
                body.roundedSize = size;
                body.size = size;

                if (size >= m_smallMsgSize ) {
                    /* add it to the write conflict resolution table
                     * and align the boundaries */
                    m_msgsort.pushWrite( id, body.dstSlot,
                            body.roundedDstOffset, body.roundedSize );
                }
                else
                {
                    body.roundedSize = 0;
                }
                /* store it in a lookup table */
                m_bodyRequests[ id ] = body;

                /* Send a request out for the edge */
                Edge edge ;
                edge.id = id;
#ifdef LPF_CORE_MPI_USES_mpimsg
                edge.tag = -1;
#endif
                edge.canWriteHead = false;
                edge.canWriteTail = false;
                edge.srcPid = srcPid;
                edge.dstPid = dstPid;
                edge.srcSlot = srcSlot;
                edge.dstSlot = dstSlot;
                edge.srcOffset = srcOffset;
                edge.dstOffset = dstOffset;
                edge.bufOffset = static_cast<size_t>(-1);
                edge.size = size;
                edge.roundedDstOffset = body.roundedDstOffset;
                edge.roundedSize = body.roundedSize;
                m_edgeRecv[id] = edge;

                break;
            }

            default: ASSERT(!"Unexpected message"); break;
        }
    }

    LOG(4, "Processing message edges" );

    /* Figure out which edge requests require further processing */
    const size_t localNumberOfEdges = newMsgId;
    for (size_t id = 0 ; id < localNumberOfEdges; ++id )
    {
        Edge & edge = m_edgeRecv[id];

        size_t headSize = edge.roundedDstOffset - edge.dstOffset;
        size_t tailSize = edge.size - edge.roundedSize - headSize;

        bool canWriteHead = headSize > 0
            && m_msgsort.canWrite( id, edge.dstSlot, edge.dstOffset);

        bool canWriteTail = tailSize > 0
            && m_msgsort.canWrite( id, edge.dstSlot, edge.dstOffset + edge.size-1) ;

        if ( canWriteHead || canWriteTail )
        {
            edge.bufOffset = m_edgeBuffer.size();
#ifdef LPF_CORE_MPI_USES_mpimsg
            edge.tag = tagger;
            tagger += (canWriteHead + canWriteTail );
#endif
            edge.canWriteHead = canWriteHead;
            edge.canWriteTail = canWriteTail;

            m_edgeBuffer.resize( m_edgeBuffer.size() +
                (canWriteHead ? headSize : 0) +
                (canWriteTail ? tailSize : 0) );

#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs
            if ( !m_memreg.isLocalSlot( edge.dstSlot ) )  /* was this from a put?*/
#endif
            {
                newMsg( HpEdges, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() )
                    .write( MsgId, edge.id)
#ifdef LPF_CORE_MPI_USES_mpimsg
                    .write( Tag, edge.tag )
#endif
                    .write( Head, edge.canWriteHead )
                    .write( Tail, edge.canWriteTail )
                    .write( SrcPid, edge.srcPid )
                    .write( DstPid, edge.dstPid )
                    .write( SrcSlot, edge.srcSlot )
                    .write( DstSlot, edge.dstSlot )
                    .write( SrcOffset, edge.srcOffset )
                    .write( DstOffset, edge.dstOffset )
                    .write( BufOffset, edge.bufOffset )
                    .write( RoundedDstOffset, edge.roundedDstOffset )
                    .write( RoundedSize, edge.roundedSize )
                    .write( Size, edge.size )
                    .send( *m_secondQueue, edge.srcPid );
            }
        }

        ASSERT( !edge.canWriteHead || edge.bufOffset + headSize <= m_edgeBuffer.size() );
        ASSERT( !edge.canWriteTail || edge.bufOffset + (edge.canWriteHead?headSize:0)
                                          + tailSize <= m_edgeBuffer.size() );
    }

    ASSERT( m_bodyRecvs.empty() );

    LOG(4, "Resolving write conflicts" );

    // 3. Read out the conflict free message requests, and adjust them
    // note: this may double the number of messages!
    { MessageSort::MsgId msgId = 0; char * addr = 0; size_t size = 0;
    while ( m_msgsort.popWrite( msgId, addr, size ) )
    {
        Body body = m_bodyRequests[ msgId ];

        /* Note: Get's and put's are handled the same */

        ASSERT( body.dstPid == static_cast<pid_t>(m_pid) );
        ASSERT( body.srcPid != static_cast<pid_t>(m_pid) );

        char * origRoundedAddr = static_cast<char *>(
                    m_memreg.getAddress( body.dstSlot, body.roundedDstOffset)
                );
        ptrdiff_t shift = addr - origRoundedAddr ;

        Body bodyPart = body;
        bodyPart.roundedDstOffset += shift ;
        bodyPart.roundedSize = size;

#ifdef LPF_CORE_MPI_USES_mpimsg
        bodyPart.tag = tagger++; // generate unique ids for MPI message tags
#endif

#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs
        if ( m_memreg.isLocalSlot( bodyPart.dstSlot) ) /* handle gets at their dest */
#endif
        {
            m_bodyRecvs.push_back( bodyPart );
        }
#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs
        else                                           /* handle puts at their src */
#endif
        {
            newMsg( HpBodyReply, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() )
                .write( MsgId, bodyPart.id )
#ifdef LPF_CORE_MPI_USES_mpimsg
                .write( Tag, bodyPart.tag )
#endif
                .write( SrcPid, bodyPart.srcPid )
                .write( DstPid, bodyPart.dstPid )
                .write( SrcSlot, bodyPart.srcSlot )
                .write( DstSlot, bodyPart.dstSlot )
                .write( SrcOffset, bodyPart.srcOffset )
                .write( DstOffset, bodyPart.dstOffset )
                .write( Size, bodyPart.size )
                .write( RoundedDstOffset, bodyPart.roundedDstOffset )
                .write( RoundedSize, bodyPart.roundedSize )
                .send( *m_secondQueue, body.srcPid );
        }
   } }

    // 4. exchange the messages to their destination
    LOG(4, "Executing 2nd meta-data exchange");
    if ( m_secondQueue->exchange( m_comm, randomize, m_vote.data(), trials )) {
        LOG(2, "All " << trials << " sparse all-to-all attempts have failed");
        throw std::runtime_error("All sparse all-to-all attempts have failed");
    }

    ASSERT( m_bodySends.empty() );
    ASSERT( m_edgeSend.empty() );

    LOG(4, "Processing message meta-data" );
    // 5. Execute buffered gets and process get edges
    //  postpone unbuffered comm just a little while.
    while( !m_secondQueue->empty() )
    {
        mpi::IPCMesg<Msgs> msg = recvMsg<Msgs>( *m_secondQueue, m_tinyMsgBuf.data(), m_tinyMsgBuf.size() );

        switch ( msg.type() )
        {
            case BufGetReply: { /* handle the response of a buffered get */
                memslot_t dstSlot;
                size_t dstOffset;
                msg.read( DstSlot, dstSlot)
                   .read( DstOffset, dstOffset );

                void * addr = m_memreg.getAddress( dstSlot, dstOffset);

                msg.read( Payload, addr, msg.bytesLeft() );
                break;
            }

            case HpEdges : {
                Edge e ;
                msg .read( MsgId, e.id)
#ifdef LPF_CORE_MPI_USES_mpimsg
                    .read( Tag, e.tag )
#endif
                    .read( Head, e.canWriteHead )
                    .read( Tail, e.canWriteTail )
                    .read( SrcPid, e.srcPid )
                    .read( DstPid, e.dstPid )
                    .read( SrcSlot, e.srcSlot )
                    .read( DstSlot, e.dstSlot )
                    .read( SrcOffset, e.srcOffset )
                    .read( DstOffset, e.dstOffset )
                    .read( BufOffset, e.bufOffset )
                    .read( RoundedDstOffset, e.roundedDstOffset )
                    .read( RoundedSize, e.roundedSize )
                    .read( Size, e.size );
                m_edgeSend.push_back( e );
                break;
            }

            case HpBodyReply: { /* handle all unbuffered comm */
                Body bodyPart;
                msg .read( MsgId, bodyPart.id )
#ifdef LPF_CORE_MPI_USES_mpimsg
                    .read( Tag, bodyPart.tag )
#endif
                    .read( SrcPid, bodyPart.srcPid )
                    .read( DstPid, bodyPart.dstPid )
                    .read( SrcSlot, bodyPart.srcSlot )
                    .read( DstSlot, bodyPart.dstSlot )
                    .read( SrcOffset, bodyPart.srcOffset )
                    .read( DstOffset, bodyPart.dstOffset )
                    .read( Size, bodyPart.size )
                    .read( RoundedDstOffset, bodyPart.roundedDstOffset )
                    .read( RoundedSize, bodyPart.roundedSize );

                m_bodySends.push_back( bodyPart );
                break;
            }

            default:
                ASSERT( !"Unexpected message" );
                break;
        }
    }

#ifdef LPF_CORE_MPI_USES_mpirma
    // Make sure that no MPI put or was operating before this line
    if (m_nprocs > 1)
        m_comm.fenceAll();
#endif

    LOG(4, "Exchanging large payloads ");
    // 6. Execute unbuffered communications
    const size_t maxInt = std::numeric_limits<int>::max();

    for (size_t i = 0; i < localNumberOfEdges; ++i)
    {
        Edge & e = m_edgeRecv[i];
        size_t headSize = e.roundedDstOffset - e.dstOffset ;
        size_t tailSize = e.size - e.roundedSize - headSize ;
#if defined LPF_CORE_MPI_USES_mpimsg || defined LPF_CORE_MPI_USES_mpirma
        char * head = m_edgeBuffer.data() + e.bufOffset;
        char * tail = head + (e.canWriteHead?headSize:0);
#endif
#ifdef LPF_CORE_MPI_USES_mpirma
        if ( m_memreg.isLocalSlot( e.dstSlot ) ) {
            size_t tailOffset = e.roundedDstOffset + e.roundedSize
                  - e.dstOffset + e.srcOffset;

            if (e.canWriteHead) {
                m_comm.get( e.srcPid, m_memreg.getWindow( e.srcSlot),
                        e.srcOffset, head, headSize );
            }

            if (e.canWriteTail) {
                m_comm.get( e.srcPid, m_memreg.getWindow( e.srcSlot),
                        tailOffset, tail, tailSize );
            }
        }
#endif
#ifdef LPF_CORE_MPI_USES_ibverbs
        if ( m_memreg.isLocalSlot( e.dstSlot ) ) {
            size_t tailOffset = e.roundedDstOffset + e.roundedSize
                  - e.dstOffset + e.srcOffset;

            if (e.canWriteHead) {
                m_ibverbs.get( e.srcPid, m_memreg.getVerbID( e.srcSlot),
                        e.srcOffset,
                        m_memreg.getVerbID( m_edgeBufferSlot ), e.bufOffset,
                        headSize );
            }

            if (e.canWriteTail) {
                m_ibverbs.get( e.srcPid, m_memreg.getVerbID( e.srcSlot),
                        tailOffset,
                        m_memreg.getVerbID( m_edgeBufferSlot ),
                        e.bufOffset + (e.canWriteHead?headSize:0),
                        tailSize );
            }
        }
#endif
#ifdef LPF_CORE_MPI_USES_mpimsg
        if (e.canWriteHead)
            m_comm.irecv( head, headSize, e.srcPid, e.tag );

        if (e.canWriteTail)
            m_comm.irecv( tail, tailSize, e.srcPid, e.tag + e.canWriteHead );
#endif
    }
    /* note: maintain m_edgeRecv until they have been copied */

#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs
    ASSERT( m_edgeBufferSlot == m_memreg.invalidSlot()
            || m_memreg.getAddress(m_edgeBufferSlot, 0) == m_edgeBuffer.data() );
    ASSERT( m_edgeBufferSlot == m_memreg.invalidSlot()
            ||m_memreg.getSize(m_edgeBufferSlot) == m_edgeBuffer.capacity() );
#endif
    for (size_t i = 0; i < m_edgeSend.size(); ++i)
    {
        Edge & e = m_edgeSend[i];
        size_t headSize = e.roundedDstOffset - e.dstOffset ;
        size_t tailOffset = e.roundedDstOffset + e.roundedSize - e.dstOffset;
        size_t tailSize = e.size - headSize - e.roundedSize ;

#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_mpimsg
        char * head = static_cast<char *>(
                m_memreg.getAddress( e.srcSlot, e.srcOffset)
                );

        char * tail = head + tailOffset;
#endif
#ifdef LPF_CORE_MPI_USES_mpirma
        ASSERT( ! m_memreg.isLocalSlot( e.dstSlot ) ) ;
        if (e.canWriteHead)
            m_comm.put( head, e.dstPid, m_memreg.getWindow( m_edgeBufferSlot ),
                    e.bufOffset, headSize );

        if (e.canWriteTail)
            m_comm.put( tail, e.dstPid, m_memreg.getWindow( m_edgeBufferSlot ),
                    e.bufOffset + (e.canWriteHead?headSize:0), tailSize);
#endif
#ifdef LPF_CORE_MPI_USES_ibverbs
        ASSERT( ! m_memreg.isLocalSlot( e.dstSlot ) ) ;
        if (e.canWriteHead)
            m_ibverbs.put( m_memreg.getVerbID( e.srcSlot), e.srcOffset,
                    e.dstPid, m_memreg.getVerbID( m_edgeBufferSlot ),
                    e.bufOffset, headSize );

        if (e.canWriteTail)
            m_ibverbs.put( m_memreg.getVerbID( e.srcSlot),
                    e.srcOffset + tailOffset ,
                    e.dstPid, m_memreg.getVerbID( m_edgeBufferSlot ),
                    e.bufOffset + (e.canWriteHead?headSize:0), tailSize);
#endif
#ifdef LPF_CORE_MPI_USES_mpimsg
        if (e.canWriteHead)
            m_comm.isend( head, headSize, e.dstPid, e.tag );

        if (e.canWriteTail)
            m_comm.isend( tail, tailSize, e.dstPid, e.tag + e.canWriteHead );
#endif
    }
    m_edgeSend.clear();

    for (size_t i = 0; i < m_bodyRecvs.size() ; ++i )
    {
        Body & r = m_bodyRecvs[i];
        ASSERT( r.size > 0 );
        ASSERT( maxInt > 0 );
#if defined LPF_CORE_MPI_USES_mpimsg || defined LPF_CORE_MPI_USES_mpirma
        char * addr = static_cast<char *>(
                m_memreg.getAddress( r.dstSlot, r.roundedDstOffset)
                );
#endif
#ifdef LPF_CORE_MPI_USES_mpirma
        size_t shift = r.roundedDstOffset - r.dstOffset;
        m_comm.get( r.srcPid,
            m_memreg.getWindow( r.srcSlot),
            r.srcOffset + shift,
            addr,
            r.roundedSize );
#endif
#ifdef LPF_CORE_MPI_USES_ibverbs
        size_t shift = r.roundedDstOffset - r.dstOffset;
        m_ibverbs.get( r.srcPid,
            m_memreg.getVerbID( r.srcSlot),
            r.srcOffset + shift,
            m_memreg.getVerbID( r.dstSlot), r.roundedDstOffset,
            r.roundedSize );
#endif
#ifdef LPF_CORE_MPI_USES_mpimsg
        ASSERT( r.tag < maxInt );
        m_comm.irecv( addr, r.roundedSize, r.srcPid, r.tag );
#endif
    }
    m_bodyRecvs.clear();

    for (size_t i = 0; i < m_bodySends.size() ; ++i )
    {
        Body & r = m_bodySends[i];
        ASSERT( r.size > 0 );
        ASSERT( maxInt > 0 );
        size_t shift = r.roundedDstOffset - r.dstOffset;
#if defined LPF_CORE_MPI_USES_mpimsg || defined LPF_CORE_MPI_USES_mpirma
        char * addr = static_cast<char *>(
                m_memreg.getAddress( r.srcSlot, r.srcOffset + shift)
                );
#endif
#ifdef LPF_CORE_MPI_USES_mpirma
        m_comm.put( addr,
            r.dstPid,
            m_memreg.getWindow( r.dstSlot),
            r.roundedDstOffset,
            r.roundedSize );
#endif
#ifdef LPF_CORE_MPI_USES_ibverbs
        m_ibverbs.put( m_memreg.getVerbID( r.srcSlot),
            r.srcOffset + shift,
            r.dstPid,
            m_memreg.getVerbID( r.dstSlot),
            r.roundedDstOffset,
            r.roundedSize );
#endif
#ifdef LPF_CORE_MPI_USES_mpimsg
        ASSERT( r.tag < maxInt );
        m_comm.isend( addr, r.roundedSize, r.dstPid, r.tag );
#endif
    }
    m_bodySends.clear();

#ifdef LPF_CORE_MPI_USES_mpimsg
    m_comm.iwaitall();
#endif

#ifdef LPF_CORE_MPI_USES_mpirma
    // Make sure that all MPI puts and gets have finished
    if (m_nprocs > 1)
        m_comm.fenceAll();
#endif
#ifdef LPF_CORE_MPI_USES_ibverbs
    m_ibverbs.sync( m_resized );
#endif
    LOG(4, "Copying edges" );

    /* 8. now copy the edges */
    for (size_t i = 0; i < localNumberOfEdges; ++i)
    {
        Edge & edge = m_edgeRecv[i];
        ASSERT( edge.size != 0);
        char * addr = static_cast<char *>(
                m_memreg.getAddress( edge.dstSlot, edge.dstOffset)
                );
        size_t size = edge.size;
        size_t headSize = edge.roundedDstOffset - edge.dstOffset ;
        size_t tailSize = edge.size - headSize - edge.roundedSize ;

        ASSERT( !edge.canWriteHead || edge.bufOffset + headSize <= m_edgeBuffer.size() );
        ASSERT( !edge.canWriteTail || edge.bufOffset + (edge.canWriteHead?headSize:0)
                                        + tailSize <= m_edgeBuffer.size() );

        char * head = m_edgeBuffer.data() + edge.bufOffset;
        char * tail = head + (edge.canWriteHead?headSize:0);
        if (edge.canWriteHead)
            std::memcpy( addr, head, headSize);

        if (edge.canWriteTail)
            std::memcpy( addr + size - tailSize , tail, tailSize );
    }

    LOG(4, "Cleaning up");

    m_firstQueue->clear();
    m_secondQueue->clear();
    m_edgeBuffer.clear();
    m_resized = false;
    ASSERT( m_firstQueue->empty() );
    ASSERT( m_secondQueue->empty() );
    ASSERT( m_msgsort.empty() );
    ASSERT( m_edgeSend.empty() );
    ASSERT( m_edgeBuffer.empty() );
    ASSERT( m_bodySends.empty() );
    ASSERT( m_bodyRecvs.empty() );

    LOG(4, "End of synchronisation");
#endif
    return 0;

}

int MessageQueue :: countingSyncPerSlot(SlotID slot, size_t expected_sent, size_t expected_rcvd)
{

    ASSERT(slot != LPF_INVALID_MEMSLOT);
    (void) expected_sent;
    (void) expected_rcvd;
#ifdef LPF_CORE_MPI_USES_zero

    // if not, deal with normal sync
    m_memreg.sync();
	m_ibverbs.countingSyncPerSlot(slot, expected_sent, expected_rcvd);
    m_resized = false;


#endif
	return 0;
}

int MessageQueue :: syncPerSlot(SlotID slot)
{

    ASSERT(slot != LPF_INVALID_MEMSLOT);
#ifdef LPF_CORE_MPI_USES_zero

    // if not, deal with normal sync
    m_memreg.sync();
	m_ibverbs.syncPerSlot(slot);
    m_resized = false;

#endif
	return 0;
}


void MessageQueue :: getRcvdMsgCountPerSlot(size_t * msgs, SlotID slot)
{

    ASSERT(msgs != nullptr);
    ASSERT(slot != LPF_INVALID_MEMSLOT);
#ifdef LPF_CORE_MPI_USES_zero
    *msgs = 0;
    m_ibverbs.get_rcvd_msg_count_per_slot(msgs, slot);
#endif
}

void MessageQueue :: getRcvdMsgCount(size_t * msgs)
{
    ASSERT(msgs != nullptr);
#ifdef LPF_CORE_MPI_USES_zero
    *msgs = 0;
    m_ibverbs.get_rcvd_msg_count(msgs);
#endif
}

void MessageQueue :: getSentMsgCount(size_t * msgs)
{
    ASSERT(msgs != nullptr);
#ifdef LPF_CORE_MPI_USES_zero
    *msgs = 0;
    m_ibverbs.get_sent_msg_count(msgs);
#endif
}

void MessageQueue :: getSentMsgCountPerSlot(size_t * msgs, SlotID slot)
{
    ASSERT(msgs != nullptr);
    ASSERT(slot != LPF_INVALID_MEMSLOT);
#ifdef LPF_CORE_MPI_USES_zero
    *msgs = 0;
    m_ibverbs.get_sent_msg_count_per_slot(msgs, slot);
#endif
}

void MessageQueue :: flushSent()
{
#ifdef LPF_CORE_MPI_USES_zero
        m_ibverbs.flushSent();
#endif
}

void MessageQueue :: flushReceived()
{
#ifdef LPF_CORE_MPI_USES_zero
        m_ibverbs.flushReceived();
#endif
}


} // namespace lpf

