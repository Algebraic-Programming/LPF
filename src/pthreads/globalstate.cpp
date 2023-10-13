
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

#include "globalstate.hpp"
#include <log.hpp>
#include <assert.hpp>
#include <config.hpp>

namespace lpf {

GlobalState:: GlobalState( pid_t nprocs )
    : m_nprocs( nprocs )
    , m_barrier( nprocs, 
           Config::instance().getSpinMode() == Config::SPIN_FAST ? Barrier::SPIN_FAST :
           Config::instance().getSpinMode() == Config::SPIN_PAUSE ? Barrier::SPIN_PAUSE :
           Config::instance().getSpinMode() == Config::SPIN_COND ? Barrier::SPIN_COND :
           Barrier::SPIN_YIELD )
    , m_msgQueue( nprocs )
    , m_nextMsgQueueCapacity(nprocs, 0)
    , m_msgQueueCapacity( nprocs, 0 )
    , m_register( nprocs )
    , m_nextRegisterCapacity(nprocs, 0)
{
}

void GlobalState :: init( pid_t myPid )
{
    m_barrier.init( myPid );
    m_msgQueue.init( myPid );
}

void GlobalState :: destroy( pid_t myPid )
{
    m_msgQueue.destroy( myPid );
    m_register[myPid].destroy( );
}

bool GlobalState :: sync( pid_t myPid )
{
    ASSERT( myPid < m_nprocs );
    if (m_barrier.execute(myPid)) return true;
    ////////////////  READ ONLY SECTION  ////////////////////////////////////////

    m_msgQueue.execute( myPid, m_register );

    ////////////////  END READ ONLY /////////////////////////////////////////////
    if (m_barrier.execute(myPid)) return true;
    
    // clear the message buffer
    m_msgQueue.clear( myPid );

    // shrink the memory register if it was necessary
    ASSERT( m_nextRegisterCapacity[ myPid ] <= m_register[myPid].capacity() );
    if ( m_nextRegisterCapacity[ myPid ] < m_register[myPid].capacity() )
    {
        m_register[myPid].reserve( m_nextRegisterCapacity[ myPid ] );
    }

    // and shrink the message queue if that was scheduled in last superstep
    if (m_nextMsgQueueCapacity[myPid] != size_t(-1)) {
        m_msgQueue.reserve( myPid, m_register[myPid].capacity(),
                m_nextMsgQueueCapacity[ myPid ], true );
        m_msgQueueCapacity[ myPid ] = m_nextMsgQueueCapacity[ myPid ];
        m_nextMsgQueueCapacity[myPid] = size_t(-1);
    }
    return false;
}

void GlobalState :: put( pid_t srcPid, memslot_t srcSlot, size_t srcOffset, 
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
            size_t size )
{
    m_msgQueue.push( srcPid, srcPid,srcSlot, srcOffset, 
            dstPid, dstSlot, dstOffset, size, m_register );
}

void GlobalState :: get( pid_t srcPid, memslot_t srcSlot, size_t srcOffset, 
            pid_t dstPid, memslot_t dstSlot, size_t dstOffset,
            size_t size )
{
    m_msgQueue.push( dstPid, srcPid, srcSlot, srcOffset, 
            dstPid, dstSlot, dstOffset, size, m_register );
}

memslot_t GlobalState :: registerGlobal( pid_t pid, void * mem, size_t size )
{
    (void) size;
    Memory rec = { 
        static_cast<char *>(mem), 
#ifndef NDEBUG
        size 
#endif
    };
    return m_register[pid].addGlobalReg( rec );
}

memslot_t GlobalState :: registerLocal( pid_t pid, void * mem, size_t size)
{ 
    (void) size;
    Memory rec = { 
        static_cast<char *>(mem), 
#ifndef NDEBUG
        size 
#endif
    };
    return m_register[pid].addLocalReg( rec );
}

void GlobalState :: deregister( pid_t pid, memslot_t slot )
{
    m_register[pid].removeReg( slot );
}

err_t GlobalState :: resizeMesgQueue( pid_t pid, size_t nMsgs )
{
    try
    {
        size_t maxregs = m_register[pid].capacity();
        m_msgQueue.reserve( pid, maxregs, nMsgs, false );
        if ( nMsgs > m_msgQueueCapacity[pid] )
            m_msgQueueCapacity[pid] = nMsgs;
        
        // possibly delay shrinking 
        m_nextMsgQueueCapacity[ pid ] = nMsgs;
    }
    catch( std::bad_alloc & )
    {
        LOG(2, "Unable to increase message queue capacity");
        return LPF_ERR_OUT_OF_MEMORY;
    }
    return LPF_SUCCESS;
}

err_t GlobalState :: resizeMemreg( pid_t pid, size_t nRegs )
{
    try
    {
        if ( nRegs > m_register[pid].capacity() )
        {
            m_register[pid].reserve( nRegs );
            size_t nMsgs = m_msgQueueCapacity[pid];
            m_msgQueue.reserve( pid, nRegs, nMsgs, false);
        }
        // else delay reducing capacity to next sync
        
        m_nextRegisterCapacity[ pid ] = nRegs;
    }
    catch( std::bad_alloc & )
    {
        LOG(2, "Unable to increase memory registry capacity");
        return LPF_ERR_OUT_OF_MEMORY;
    }
    return LPF_SUCCESS;

}


} // namespace lpf

