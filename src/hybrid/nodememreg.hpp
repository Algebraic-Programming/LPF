
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

#ifndef LPFLIB_NODEMEMREG_HPP
#define LPFLIB_NODEMEMREG_HPP

#include "sparseset.hpp"
#include "memreg.hpp"
#include "dispatch.hpp"
#include "linkage.hpp"

namespace lpf { namespace hybrid {


class _LPFLIB_LOCAL NodeMemReg
{
public:
     struct Memory {
        void * m_addr;
        size_t m_size;
        Thread::memslot_t m_threadSlot;
        MPI::memslot_t m_nodeSlot;

        Memory()
            : m_addr(NULL), m_size(0)
            , m_threadSlot( Thread::INVALID_MEMSLOT )
            , m_nodeSlot( Thread::INVALID_MEMSLOT )
        {}

        Memory( void * addr, size_t size, 
                Thread::memslot_t threadSlot) 
            : m_addr( addr )
            , m_size( size )
            , m_threadSlot( threadSlot )
            , m_nodeSlot( MPI::INVALID_MEMSLOT )
        {}
    };

private:
    typedef CombinedMemoryRegister<Memory> MemReg;

public:
    typedef MemReg::Slot Slot;
    static Slot invalidSlot() { return MemReg::invalidSlot(); }

    NodeMemReg( size_t nRealThreads, size_t nMaxThreads )
        : m_nRealThreads( nRealThreads )
        , m_nMaxThreads( nMaxThreads )
        , m_regs( nMaxThreads )
        , m_regsAdded( nRealThreads, SparseSet<Slot>(0,0) )
        , m_regsRemoved( nRealThreads, SparseSet<Slot>(0,0) )
    {}

    void reserve( size_t threadId, size_t nRegs )
    {
        m_regs[threadId].reserve( 2*nRegs );
        m_regsAdded[threadId].resize( m_regs[threadId].range() );
        m_regsRemoved[threadId].resize( m_regs[threadId].range() );

        if ( threadId == 0) {
            for (size_t q = m_nRealThreads; q < m_nMaxThreads; ++q)
                m_regs[q].reserve( 2*nRegs );
        }
    }


    Slot addGlobal( size_t threadId, void * addr, size_t size, Thread::memslot_t threadSlot )
    {       
        Slot slot = m_regs[threadId].addGlobalReg(
                Memory( addr, size, threadSlot )
                );
        m_regsAdded[threadId].insert( slot );

        if ( threadId == 0) {
            for (size_t q = m_nRealThreads; q < m_nMaxThreads; ++q)
            {
                Slot s = m_regs[q].addGlobalReg(
                        Memory( NULL, 0, Thread::INVALID_MEMSLOT)
                     );
                ASSERT( s == slot );
            }
        }
        return slot;
    }

    Slot addLocal( size_t threadId, void * addr, size_t size, Thread::memslot_t threadSlot )
    {       
        Slot slot = m_regs[threadId].addLocalReg(
                Memory( addr, size, threadSlot )
                );
        m_regsAdded[threadId].insert( slot );

        return slot;
    }

    void remove( size_t threadId, Slot slot)
    {
        if ( m_regsAdded[threadId].contains(slot) )
        {
            m_regsAdded[threadId].erase( slot );
            m_regs[threadId].removeReg( slot );

            if ( threadId == 0 && !MemReg::isLocalSlot( slot ) ) {
                for (size_t q = m_nRealThreads; q < m_nMaxThreads; ++q)
                    m_regs[q].removeReg( slot );
            }
        }
        else {
            m_regsRemoved[threadId].insert( slot );
        }
    }

    const Memory & lookup( size_t threadId, Slot slot ) const
    { return m_regs[threadId].lookup(slot); }


    void flush( MPI comm )
    {
        typedef SparseSet< Slot > :: iterator SlotIt;

        // removal of slots
        for (SlotIt slot = m_regsRemoved[0].begin();
                slot != m_regsRemoved[0].end(); ++slot )
        {
            if ( MemReg::isLocalSlot( *slot ) )
            {
                MPI::err_t rc = MPI::SUCCESS;
                const Memory & mem = m_regs[0].lookup( *slot );
                rc = comm.deregister( mem.m_nodeSlot );
                ASSERT( MPI::SUCCESS == rc );
                m_regs[0].removeReg( *slot );
            }
            else
            {
                for (size_t q = 0; q < m_nMaxThreads; ++q)
                {
                    MPI::err_t rc = MPI::SUCCESS;
                    const Memory & mem = m_regs[q].lookup( *slot );
                    rc = comm.deregister( mem.m_nodeSlot );
                    ASSERT( MPI::SUCCESS == rc );
                    m_regs[q].removeReg( *slot );
                }
            }
        }
        m_regsRemoved[0].clear();

        for (size_t q = 1; q < m_nRealThreads; ++q) {
            for (SlotIt slot = m_regsRemoved[q].begin();
                    slot != m_regsRemoved[q].end(); ++slot )
            {
                if ( MemReg::isLocalSlot( *slot ) )
                {
                    MPI::err_t rc = MPI::SUCCESS;
                    const Memory & mem = m_regs[q].lookup( *slot );
                    rc = comm.deregister( mem.m_nodeSlot );
                    ASSERT( MPI::SUCCESS == rc );
                    m_regs[q].removeReg( *slot );
                }
            }
            m_regsRemoved[q].clear();
        }


        // addition of slots
        for (SlotIt slot = m_regsAdded[0].begin();
                slot != m_regsAdded[0].end(); ++slot )
        {
            if ( MemReg::isLocalSlot( *slot ) )
            {
                MPI::err_t rc = MPI::SUCCESS;
                Memory & mem = m_regs[0].update( *slot );
                rc = comm.register_local( mem.m_addr, mem.m_size, &mem.m_nodeSlot );
                ASSERT( MPI::SUCCESS == rc );
            }
            else
            {
                Memory null( NULL, 0, Thread::INVALID_MEMSLOT );
                for (size_t q = 0; q < m_nMaxThreads; ++q)
                {
                    MPI::err_t rc = MPI::SUCCESS;
                    Memory & mem = m_regs[q].update( *slot );
                    rc = comm.register_global( mem.m_addr, mem.m_size, &mem.m_nodeSlot );
                    ASSERT( MPI::SUCCESS == rc );
                }
            }
        }
        m_regsAdded[0].clear();

        for (size_t q = 1; q < m_nRealThreads; ++q) {
            for (SlotIt slot = m_regsAdded[q].begin();
                    slot != m_regsAdded[q].end(); ++slot )
            {
                if ( MemReg::isLocalSlot( *slot ) )
                {
                    MPI::err_t rc = MPI::SUCCESS;
                    Memory & mem = m_regs[q].update( *slot );
                    rc = comm.register_local( mem.m_addr, mem.m_size, &mem.m_nodeSlot );
                    ASSERT( MPI::SUCCESS == rc );
                }

            }
            m_regsAdded[q].clear();
        }

    }


private:

    size_t                      m_nRealThreads;
    size_t                      m_nMaxThreads;
    std::vector< MemReg >       m_regs;
    std::vector< SparseSet< Slot > > m_regsAdded;
    std::vector< SparseSet< Slot > > m_regsRemoved;
};
    

} }
#endif
