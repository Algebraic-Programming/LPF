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

#ifndef LPF_CORE_MEMORYTABLES_HPP
#define LPF_CORE_MEMORYTABLES_HPP

#include "memreg.hpp"
#include "sparseset.hpp"
#include "communication.hpp"
#include "assert.hpp"
#include "linkage.hpp"

#if defined (LPF_CORE_MPI_USES_ibverbs) || defined (LPF_CORE_MPI_USES_zero)
#include "ibverbs.hpp"
#endif


#include <vector>
#include <utility>
#include <limits>

namespace lpf {


class _LPFLIB_LOCAL MemoryTable
{
#ifdef LPF_CORE_MPI_USES_mpirma
    typedef Communication::Memslot Window;
#endif

    struct Memory {
        char *addr; size_t size; 
#if defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
        mpi::IBVerbs::SlotID slot;
        Memory( void * a, size_t s, mpi::IBVerbs::SlotID sl)
            : addr(static_cast<char *>(a))
            , size(s), slot(sl) {}
        Memory() : addr(NULL), size(0u), slot(-1) {}
#else
        Memory( void * a, size_t s)
            : addr(static_cast<char *>(a))
            , size(s) {}
        Memory() : addr(NULL), size(0u) {}
#endif
    };
    typedef CombinedMemoryRegister<Memory> Register;

public:
    typedef Register::Slot Slot;

    static Slot invalidSlot() 
    { return Register::invalidSlot(); }

#if defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
    explicit MemoryTable( Communication & comm, mpi::IBVerbs & verbs );
#else
    explicit MemoryTable( Communication & comm );
#endif

    Slot addLocal( void * mem, std::size_t size ) ; // nothrow

    Slot addNoc( void * mem, std::size_t size ) ; // nothrow

    Slot addGlobal( void * mem, std::size_t size ); // nothrow
    
    void remove( Slot slot );   // nothrow

    void * getAddress( Slot slot, size_t offset ) const  // nothrow
    {   ASSERT( offset <= m_memreg.lookup(slot).size  ); 
        return m_memreg.lookup(slot).addr + offset;
    }

    size_t getSize( Slot slot ) const // nothrow
    {   return m_memreg.lookup(slot).size; }

#ifdef LPF_CORE_MPI_USES_mpirma
    Window getWindow( Slot slot ) const  // nothrow
    { return m_windows[ slot ]; }
#endif

#if defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
    mpi::IBVerbs::SlotID getVerbID( Slot slot ) const
    { return m_memreg.lookup( slot ).slot; }
#endif

    void reserve( size_t size ); // throws bad_alloc, strong safe
    size_t capacity() const;
    size_t range() const;

    bool needsSync() const;
    void sync( ) ; 

    static bool isLocalSlot( Slot slot ) 
    { return Register::isLocalSlot( slot ); }

private:

    typedef SparseSet< Slot > DirtyList;

    Register       m_memreg;
    size_t m_capacity;
#ifdef LPF_CORE_MPI_USES_mpirma
    typedef std::vector< Window > WindowTable;
    WindowTable    m_windows;
    DirtyList      m_added, m_removed;
    Communication & m_comm;
#endif

#if defined LPF_CORE_MPI_USES_ibverbs || defined LPF_CORE_MPI_USES_zero
    DirtyList      m_added;
    mpi::IBVerbs & m_ibverbs;
    Communication & m_comm;
#endif
};


} // namespace lpf


#endif
