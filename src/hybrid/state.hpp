
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

#ifndef LPFLIB_HYBRID_STATE_HPP
#define LPFLIB_HYBRID_STATE_HPP

#include <vector>
#include "dispatch.hpp"
#include "nodememreg.hpp"
#include "nodemsgqueue.hpp"
#include "machineparams.hpp"
#include "linkage.hpp"

namespace lpf { namespace hybrid {

class _LPFLIB_LOCAL ThreadState;

const lpf_t LPF_SINGLE_PROCESS = static_cast<void*>(const_cast<void **>(&LPF_ROOT)) ; 

class _LPFLIB_LOCAL NodeState {
public:
    typedef LPF_CORE_IMPL_CONFIG :: MachineParams MachineParams;

    static NodeState & root() 
    { static NodeState obj( 
                std::vector< MPI::pid_t>(1, 1), 
                MPI( MPI::ROOT, 0, 1) 
            );
      return obj;
    }

    NodeState( const std::vector< MPI::pid_t > & threadCounts, MPI mpi ) 
        : m_totalProcs( std::accumulate(threadCounts.begin(), threadCounts.end(), 0) )
        , m_nodeId( mpi.pid() )
        , m_totalNodes( threadCounts.size() )
        , m_nRealThreads( threadCounts[mpi.pid()] )
        , m_nMaxThreads( *std::max_element(threadCounts.begin(), threadCounts.end()) )
        , m_localPidStart(std::accumulate(threadCounts.begin(), threadCounts.begin()+m_nodeId, 0) )
        , m_localPidEnd( std::accumulate(threadCounts.begin(), threadCounts.begin()+m_nodeId+1, 0 ) )
        , m_memreg( m_nRealThreads, m_nMaxThreads )
        , m_msgQueue( mpi.pid(), threadCounts )
        , m_mpi(mpi )
    {
       if ( ! Thread::is_linked_correctly() || ! MPI::is_linked_correctly() ) 
       {
           LOG(0, "Hybrid engine of LPF was not linked correctly. Error codes are not distinct" );
           std::abort();
       }
    }

    MPI & mpi() 
    { return m_mpi; }

    const MPI & mpi() const
    { return m_mpi; }

    void reserveMesgQueue( size_t thread, size_t nMsgs )
    { m_msgQueue.reserve( thread, nMsgs ); }

    void reserveMemreg( size_t thread, size_t nRegs )
    { m_memreg.reserve( thread, nRegs ); }

    NodeMemReg & memreg() 
    { return m_memreg; }

    NodeMsgQueue & msgQueue()
    { return m_msgQueue; }

    lpf_pid_t nodeId() const 
    { return m_nodeId; }

    lpf_pid_t nNodes() const
    { return m_totalNodes; }

    lpf_pid_t nProcs() const
    { return m_totalProcs; }

    lpf_pid_t nMaxThreads() const
    { return m_nMaxThreads; }

    lpf_pid_t nRealThreads() const
    { return m_nRealThreads; }

    bool isLocal(lpf_pid_t pid) const
    { return pid >= m_localPidStart && pid < m_localPidEnd; }

    Thread::pid_t globalToThread(lpf_pid_t pid) const
    { return pid - m_localPidStart; }

    lpf_pid_t threadToGlobal(Thread::pid_t pid) const
    { return pid + m_localPidStart; }

    MPI::err_t sync() 
    {
        m_memreg.flush( m_mpi );
        m_msgQueue.flush( m_mpi, m_memreg );
        return m_mpi.sync();
    }

    static double messageGap( lpf_pid_t nprocs, size_t minMsgSize, lpf_sync_attr_t attr)
    {
        (void) nprocs;
        (void) attr;
        return machineParams().getMessageGap( minMsgSize );
    }

    static double latency( lpf_pid_t nprocs, size_t minMsgSize, lpf_sync_attr_t attr)
    {
        (void) nprocs;
        (void) attr;
        return machineParams().getLatency(minMsgSize);
    }

    static MachineParams & machineParams()
    { static MachineParams obj; return obj; }

private:
    lpf_pid_t m_totalProcs;
    lpf_pid_t m_nodeId;
    lpf_pid_t m_totalNodes;
    Thread::pid_t m_nRealThreads;
    Thread::pid_t m_nMaxThreads;
    lpf_pid_t m_localPidStart;
    lpf_pid_t m_localPidEnd;
    NodeMemReg m_memreg;
    NodeMsgQueue m_msgQueue;
    MPI        m_mpi;
};
    

class _LPFLIB_LOCAL ThreadState {
public:

    lpf_err_t resizeMesgQueue( size_t msgs )
    {
        if ( msgs > std::numeric_limits<size_t>::max() / m_nMaxThreads )
        {
            LOG( 2, "Requested message queue size " << msgs 
                    << " exceeds theoretical capacity, because "
                       "maximum number of threads is " << m_nMaxThreads );
            return LPF_ERR_OUT_OF_MEMORY;
        }


        if ( m_threadId == 0 )
        {
            MPI::err_t rc = 
                m_nodeState.mpi().resize_message_queue(
                    msgs * m_nMaxThreads);
            if ( rc == MPI::ERR_OUT_OF_MEMORY ) {
                LOG( 2, "Thread 0 on node " << m_nodeState.nodeId() << " cannot allocate "
                        << msgs << " x " << m_nMaxThreads << " inter-node messages on "
                        "inter-node message layer" );
                return LPF_ERR_OUT_OF_MEMORY;
            }
            else if (rc != MPI::SUCCESS) {
                LOG( 2, "Thread 0 on node " << m_nodeState.nodeId() << " encountered "
                        " unexpected fatal state while allocate messages on inter-node"
                        " message layer");
                return LPF_ERR_FATAL;
            }
        }
        try {
            m_nodeState.reserveMesgQueue( m_threadId, msgs );
        }
        catch( std::bad_alloc & e)
        {
            LOG( 2, "Thread " << m_threadId << " on node " << m_nodeState.nodeId() 
                    << " cannot allocate " << msgs << " inter-node messages" );
            return LPF_ERR_OUT_OF_MEMORY;
        }
        
        Thread::err_t rc =  m_thread.resize_message_queue( msgs );

        if ( rc == Thread::ERR_OUT_OF_MEMORY ) {
            LOG( 2, "Thread " << m_threadId << " on node " << m_nodeState.nodeId() 
                    << " cannot allocate " << msgs << " intra-node messages on "
                    "intra-node message layer" );
            return LPF_ERR_OUT_OF_MEMORY;
        }
        else if (rc != Thread::SUCCESS ) {
            LOG( 2, "Thread " << m_threadId << " on node " << m_nodeState.nodeId() << " encountered "
                    " unexpected fatal state while allocating messages on intra-node"
                    " message layer");
            return LPF_ERR_FATAL;
        }
        else return LPF_SUCCESS;
    }

    lpf_err_t resizeMemreg( size_t regs )
    {
        if ( regs > std::numeric_limits<size_t>::max() / m_nMaxThreads )
        {
            LOG( 2, "Requested memory register size " << regs 
                    << " exceeds theoretical capacity, because "
                       "maximum number of threads is " << m_nMaxThreads );
            return LPF_ERR_OUT_OF_MEMORY;
        }

        if ( m_threadId == 0 )
        {
            MPI::err_t rc =
                m_nodeState.mpi().resize_memory_register(
                    regs * m_nMaxThreads );
            if ( rc == MPI::ERR_OUT_OF_MEMORY ) {
                LOG( 2, "Thread 0 on node " << m_nodeState.nodeId() << " cannot allocate "
                        << regs << " x " << m_nMaxThreads << " DRMA registers on "
                        "inter-node message layer" );
                return LPF_ERR_OUT_OF_MEMORY;
            }
            else if (rc != MPI::SUCCESS) {
                LOG( 2, "Thread 0 on node " << m_nodeState.nodeId() << " encountered "
                        " unexpected fatal state while DRMA registers on inter-node"
                        " message layer");
                return LPF_ERR_FATAL;
            }
        }
        try {
            m_nodeState.reserveMemreg( m_threadId, regs );
        }
        catch( std::bad_alloc & e)
        {
            LOG( 2, "Thread " << m_threadId << " on node " << m_nodeState.nodeId() 
                    << " cannot allocate "
                    << regs << " DRMA registers on inter-node messages" );
            return LPF_ERR_OUT_OF_MEMORY;
        }
        
        Thread::err_t rc =  m_thread.resize_memory_register( regs );

        if ( rc == Thread::ERR_OUT_OF_MEMORY ) {
            LOG( 2, "Thread " << m_threadId << " on node " << m_nodeState.nodeId() 
                    << " cannot allocate " << regs << " intra-node DRMA registers on "
                    "intra-node message layer" );
            return LPF_ERR_OUT_OF_MEMORY;
        }
        else if (rc != Thread::SUCCESS ) {
            LOG( 2, "Thread " << m_threadId << " on node " << m_nodeState.nodeId() << " encountered "
                    " unexpected fatal state while allocating DRMA registers on intra-node"
                    " message layer");
            return LPF_ERR_FATAL;
        }
        else return LPF_SUCCESS;
    }

    lpf_memslot_t register_local( void * addr, size_t size )
    {
        Thread::memslot_t threadSlot = Thread::INVALID_MEMSLOT;
        Thread::err_t rc = m_thread.register_local( addr, size, &threadSlot );
        ASSERT( rc == Thread::SUCCESS);
        lpf_memslot_t slot = m_nodeState.memreg().addLocal( 
                m_threadId, addr, size, threadSlot );
        return slot;
    }

    lpf_memslot_t register_global( void * addr, size_t size )
    {
        Thread::memslot_t threadSlot = Thread::INVALID_MEMSLOT;
        Thread::err_t rc = m_thread.register_global( addr, size, &threadSlot );
        ASSERT( rc == Thread::SUCCESS);
        lpf_memslot_t slot = m_nodeState.memreg().addGlobal( 
                m_threadId, addr, size, threadSlot );
        return slot;
    }

    void deregister( lpf_memslot_t slot )
    {
        Thread::memslot_t threadSlot = 
            m_nodeState.memreg().lookup(m_threadId, slot).m_threadSlot;
        Thread::err_t rc = m_thread.deregister( threadSlot );
        ASSERT( rc == Thread::SUCCESS);

        m_nodeState.memreg().remove( m_threadId, slot );
    }

    void put( lpf_memslot_t src_slot, size_t src_offset, 
            pid_t dst_pid, lpf_memslot_t dst_slot, size_t dst_offset, 
            size_t size)
    { 
        typedef NodeMemReg::Memory Memory;
        if (size <= 0) return;

        if (m_nodeState.isLocal(dst_pid))
        {
            const NodeMemReg & memreg = m_nodeState.memreg();
            const Memory & src = memreg.lookup(m_threadId, src_slot );
            const Memory & dst = memreg.lookup(m_threadId, dst_slot );
            Thread::pid_t localDstPid = m_nodeState.globalToThread(dst_pid);
            m_thread.put( src.m_threadSlot, src_offset, 
                    localDstPid, dst.m_threadSlot, dst_offset, size);
        }
        else
        {
            m_nodeState.msgQueue().push( m_threadId,
                    NodeMsgQueue::Put( src_slot, src_offset, 
                        dst_pid, dst_slot, dst_offset,
                        size ) );
        }
    }

    void get( pid_t src_pid, lpf_memslot_t src_slot, size_t src_offset, 
            lpf_memslot_t dst_slot, size_t dst_offset,
            size_t size )
    { 
        typedef NodeMemReg::Memory Memory;
        if (size <= 0) return;

        if (m_nodeState.isLocal(src_pid))
        {
            const NodeMemReg & memreg = m_nodeState.memreg();
            const Memory & src = memreg.lookup(m_threadId, src_slot );
            const Memory & dst = memreg.lookup(m_threadId, dst_slot );
            Thread::pid_t localSrcPid = m_nodeState.globalToThread(src_pid);
            m_thread.get( localSrcPid, src.m_threadSlot, src_offset, 
                    dst.m_threadSlot, dst_offset, size);
        }
        else
        {
            m_nodeState.msgQueue().push( m_threadId,
                NodeMsgQueue::Get( src_pid, src_slot, src_offset,
                        dst_slot, dst_offset, size ));
        }
    }

    lpf_err_t sync( )
    { 
        Thread::err_t trc = Thread::SUCCESS;

        // sync (while also finishing local put&gets)
        trc = m_thread.sync();
        if (trc != Thread::SUCCESS) {
            m_error = true;
            return LPF_ERR_FATAL;
        }

        // do the inter-node communication
        if (m_threadId == 0)
        {
            MPI::err_t nrc = m_nodeState.sync();
            if (nrc != MPI::SUCCESS) {
                m_error = true;
                return LPF_ERR_FATAL;
            }
        }

        // other threads wait until the thread 0 has finished
        trc = m_thread.sync();
        if (trc != Thread::SUCCESS) {
            m_error = true;
            return LPF_ERR_FATAL;
        }

        return LPF_SUCCESS;
    }

    ThreadState( NodeState * nodeState, Thread thread )
        : m_error(false)
        , m_threadId( thread.pid() )
        , m_nRealThreads( nodeState->nRealThreads() )
        , m_nMaxThreads( nodeState->nMaxThreads() )
        , m_pid(nodeState->threadToGlobal( thread.pid() ) )
        , m_nprocs(nodeState->nProcs() )
        , m_nodeState( *nodeState )
        , m_thread( thread )
    {
       if ( ! Thread::is_linked_correctly() || ! MPI::is_linked_correctly() ) 
       {
           LOG(0, "Hybrid engine of LPF was not linked correctly. Error codes are not distinct" );
           std::abort();
       }
    }

    ~ThreadState()
    {
    }

    static ThreadState & root()
    {
        static ThreadState obj( &NodeState::root(), 
                Thread( Thread::ROOT, 0, 1) );
        return obj;
    }

    lpf_pid_t threadId() const { return m_threadId; }
    lpf_pid_t nRealThreads() const { return m_nRealThreads; }
    lpf_pid_t nMaxThreads() const { return m_nMaxThreads; }
    lpf_pid_t pid() const { return m_pid; }
    lpf_pid_t nprocs() const { return m_nprocs; }
    const NodeState & nodeState() const { return m_nodeState; }
    Thread thread() const { return m_thread; }

    bool error() const { return m_error; }

    lpf_pid_t getRcvdMsgCount(size_t * rcvd_msgs) {

        return m_nodeState.mpi().get_rcvd_msg_count(rcvd_msgs);
    }

private:

    bool      m_error;
    lpf_pid_t m_threadId;
    lpf_pid_t m_nRealThreads;
    lpf_pid_t m_nMaxThreads;
    lpf_pid_t m_pid;
    lpf_pid_t m_nprocs;
    NodeState & m_nodeState;
    Thread m_thread;
};





    
} }

#endif
