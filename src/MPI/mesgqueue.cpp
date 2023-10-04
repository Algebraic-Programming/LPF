
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
#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs
    , m_edgeBufferSlot( m_memreg.invalidSlot() )
#endif
    , m_bodySends()
    , m_bodyRecvs()
    , m_comm( dynamic_cast<mpi::Comm &>(comm) )
#ifdef LPF_CORE_MPI_USES_ibverbs
    , m_ibverbs( m_comm )
    , m_memreg( m_comm, m_ibverbs )
#else
    , m_memreg( m_comm )
#endif
    , m_tinyMsgBuf( m_tinyMsgSize + largestHeader(m_nprocs, m_memRange, 0, 0))
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
#ifdef LPF_CORE_MPI_USES_ibverbs
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
#ifdef LPF_CORE_MPI_USES_ibverbs
    m_ibverbs.get(srcPid,
            m_memreg.getVerbID( srcSlot),
            srcOffset,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size );
#endif
}

void MessageQueue :: put( memslot_t srcSlot, size_t srcOffset,
        pid_t dstPid, memslot_t dstSlot, size_t dstOffset, size_t size )
{
#ifdef LPF_CORE_MPI_USES_ibverbs
    m_ibverbs.put( m_memreg.getVerbID( srcSlot),
            srcOffset,
            dstPid,
            m_memreg.getVerbID( dstSlot),
            dstOffset,
            size, m_memreg.getVerbID(dstSlot) );
#endif

}

int MessageQueue :: sync( bool abort )
{
    m_memreg.sync();

#ifdef LPF_CORE_MPI_USES_ibverbs
	m_ibverbs.sync( m_resized);
#endif

	m_resized = false;

	return 0;
}


void MessageQueue :: getRcvdMsgCount(size_t * msgs, SlotID slot)
{
    *msgs = 0;
#ifdef LPF_CORE_MPI_USES_ibverbs
        m_ibverbs.get_rcvd_msg_count(msgs, slot);
#endif
}


} // namespace lpf

