
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

#include "memorytable.hpp"
#include "communication.hpp"

#include <cmath>

namespace lpf {

MemoryTable :: MemoryTable( Communication & comm
#ifdef LPF_CORE_MPI_USES_ibverbs
        , mpi::IBVerbs & ibverbs
#endif
        )
    : m_memreg()
    , m_capacity( 0 )
#ifdef LPF_CORE_MPI_USES_mpirma
    , m_added( 0, 0 )
    , m_removed( 0, 0 )
    , m_comm( comm )
#endif
#ifdef LPF_CORE_MPI_USES_ibverbs
    , m_added( 0, 0 )
    , m_ibverbs( ibverbs )
    , m_comm( comm )
#endif
{ (void) comm; }


MemoryTable :: Slot
MemoryTable :: addLocal( void * mem, std::size_t size )  // nothrow
{
#ifdef LPF_CORE_MPI_USES_ibverbs
    Memory rec( mem, size, m_ibverbs.regLocal( mem, size));
#else
    Memory rec( mem, size);
#endif
    return m_memreg.addLocalReg( rec);
}

MemoryTable :: Slot
MemoryTable :: addGlobal( void * mem, std::size_t size ) // nothrow
{ 
#ifdef LPF_CORE_MPI_USES_ibverbs
    Memory rec(mem, size, -1); 
#else
    Memory rec(mem, size); 
#endif
    Slot slot = m_memreg.addGlobalReg(rec) ; 
#if defined LPF_CORE_MPI_USES_mpirma || defined LPF_CORE_MPI_USES_ibverbs
    m_added.insert( slot );
#endif
    return slot;
}

void MemoryTable :: remove( Slot slot )   // nothrow
{
#ifdef LPF_CORE_MPI_USES_mpirma
    if ( isLocalSlot(slot) )
    {
        m_memreg.removeReg( slot );
    }
    else
    {
        if (m_added.contains(slot)) {
            // an addition is undone
            m_added.erase( slot );
            m_memreg.removeReg( slot );
        }
        else {
            m_removed.insert(slot);
        }
    }
#endif

#ifdef LPF_CORE_MPI_USES_mpimsg
    m_memreg.removeReg( slot );
#endif

#ifdef LPF_CORE_MPI_USES_ibverbs
    if (m_added.contains(slot)) {
        m_added.erase(slot);
    }
    else {
        m_ibverbs.dereg( m_memreg.lookup(slot).slot );
    }
    m_memreg.removeReg( slot );
#endif
}


void MemoryTable :: reserve( size_t size ) // throws bad_alloc, strong safe
{
#ifdef LPF_CORE_MPI_USES_mpirma
    // Note: the we reserve twice the maximum number of registers because
    // we want to cover the fact that deregistered slots are actually only
    // freed up after the next sync(), while they should be promptly available
    // right after a remove().
    m_memreg.reserve( 2*size );
    size_t range = m_memreg.range();
    m_added.resize( range );
    m_removed.resize( range );
    m_windows.resize( range );
    m_comm.reserveMemslots( range );
#endif

#ifdef LPF_CORE_MPI_USES_mpimsg
    m_memreg.reserve( size );
#endif

#ifdef LPF_CORE_MPI_USES_ibverbs
    m_memreg.reserve( size );
    size_t range = m_memreg.range();
    m_added.resize( range );
    m_ibverbs.resizeMemreg( size );
#endif

    m_capacity = size;
}

size_t MemoryTable :: capacity() const
{
    return m_capacity;
}

size_t MemoryTable :: range() const
{ 
    return m_memreg.range();
}

bool MemoryTable :: needsSync() const
{ 
#ifdef LPF_CORE_MPI_USES_mpirma
    return ! m_added.empty() || !m_removed.empty();
#endif
#ifdef LPF_CORE_MPI_USES_mpimsg
    return false;
#endif
#ifdef LPF_CORE_MPI_USES_ibverbs
    return !m_added.empty();
#endif
}

void MemoryTable :: sync(  ) 
{
#ifdef LPF_CORE_MPI_USES_mpirma
    if ( !m_removed.empty() )
    {
        // delete removed MPI windows
        typedef DirtyList::iterator It;
        for ( It i = m_removed.begin(); i != m_removed.end(); ++i)
        {
            ASSERT( !isLocalSlot( *i ));
            m_comm.removeMemslot( m_windows[ *i ] );
            m_windows[*i] = Window();
            m_memreg.removeReg( *i );
        }

        // clear the removal list
        m_removed.clear();
    }

    if ( !m_added.empty() )
    {
        // create the MPI windows
        typedef DirtyList::iterator It;
        for ( It i = m_added.begin(); i != m_added.end(); ++i)
        {
            ASSERT( !isLocalSlot( *i ));
            void * base = m_memreg.lookup( *i).addr;
            size_t size = m_memreg.lookup( *i ).size;
            Window w = m_comm.createMemslot( base, size ); 
            m_windows[ *i ] = w;
            m_comm.fence( w );
        }

        // clear the added list
        m_added.clear();
    } // if 
#endif

#ifdef LPF_CORE_MPI_USES_ibverbs
    if ( !m_added.empty() )
    {
        // Register the global with IBverbs
        typedef DirtyList::iterator It;
        for ( It i = m_added.begin(); i != m_added.end(); ++i)
        {
            ASSERT( !isLocalSlot( *i ));
            void * base = m_memreg.lookup( *i).addr;
            size_t size = m_memreg.lookup( *i ).size;
            mpi::IBVerbs::SlotID s = m_ibverbs.regGlobal( base, size ); 
            m_memreg.update( *i ).slot = s;
        }

        m_comm.barrier();

        // clear the added list
        m_added.clear();
    }
#endif
}



} // namespace lpf
