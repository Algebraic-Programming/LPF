
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

#ifndef PLATFORM_CORE_MEMREG_HPP
#define PLATFORM_CORE_MEMREG_HPP

#include <vector>
#include <limits>
#include "assert.hpp"
#include "log.hpp"

namespace lpf {


/** Stores records and associates them with a slot number, which is ideal for
 * global memory registration. Memory must be reserved beforehand using
 * reserve(). As long as all processes perform calls to add()
 * and remove() in the same order, the returned slot numbers will be globally
 * consistent, regardless of any calls to reserve().
 */
template <class Record>
class MemoryRegister
{
public:
    typedef typename std::vector< Record > :: size_type Slot;
    static Slot invalidSlot() { return std::numeric_limits< Slot >::max(); }

    MemoryRegister()
        : m_slots()
        , m_free()
        , m_minNewSlotNr(0u)
    {}

    MemoryRegister( const MemoryRegister & other)
        : m_slots( other.m_slots )
        , m_free( other.m_free )
        , m_minNewSlotNr( other.m_minNewSlotNr )
    {
        m_free.reserve( other.m_free.capacity() );
    }

    // swap contents with other object
    void swap( MemoryRegister & other) // nothrow
    {
        using std::swap;
        this->m_slots.swap( other.m_slots );
        this->m_free.swap( other.m_free );
        swap( this->m_minNewSlotNr, other.m_minNewSlotNr );
    }

    MemoryRegister & operator=( const MemoryRegister & other)
    {
        MemoryRegister tmp(other);
        swap(tmp);
        return *this;
    }

    void destroy()
    {
        m_slots.clear();
        m_free.clear();
        m_minNewSlotNr = 0;
    }

    // register a memory area
    Slot add( Record record )   // nothrow
    {
        // and store the slot id.
        Slot slot = invalidSlot();
        if ( m_free.empty() )
        {
            slot = m_minNewSlotNr++;
        }
        else
        {
            ASSERT( m_free.size() <=  m_free.capacity() );
            slot = m_free.back();
            m_free.pop_back();
            ASSERT( m_free.size() <  m_free.capacity() );
        }

        // store the slot
        ASSERT( slot < m_slots.size() );
        m_slots[ slot ] = record;
        return slot;
    }

    // remove registration 
    void remove( Slot slot )  // nothrow
    {
        ASSERT( slot < m_slots.size() );
        ASSERT( m_free.size() <  m_free.capacity() );

        if ( slot == m_minNewSlotNr -1 )
        {
            m_minNewSlotNr--;
        }
        else
        {
            // push the slot back to the heap of free slots
            m_free.push_back( slot );
        }
        ASSERT( m_free.size() <=  m_free.capacity() );
    }


    // lookup the base pointer of a registered memory area
    const Record & lookup( Slot slot ) const // nothrow
    {
        ASSERT( slot < m_slots.size() );
        return m_slots[slot] ;
    }

    Record & update( Slot slot ) // nothrow
    {
        ASSERT( slot < m_slots.size() );
        return m_slots[slot] ;
    }

    Slot capacity() const
    { return m_slots.size(); }


    // ensure that 'newSize' memory areas can be registered
    void reserve( size_t newSize, const Record & defaultRecord = Record() ) 
        // throws only std::bad_alloc, strong exception safe
    {

        size_t oldSize = m_slots.size();
        if ( oldSize < newSize )
        { // The register has to grow
            if ( newSize > m_free.max_size() || newSize > m_slots.max_size() )
                throw std::bad_alloc();

            // Reserve space for the free list (bad_alloc's should propagate)
            m_free.reserve( newSize );

            // expand number of slots (might throw a bad_alloc, which must
            // propagate!)
            m_slots.resize( newSize, defaultRecord );

        }
        else
        { // The register has to shrink, but we might have to adjust
            newSize = std::max( newSize, m_minNewSlotNr );

            // resize the array containing the slots
            m_slots.resize( newSize );

            // STL vectors don't actually free their memory, unless you swap them
            // with a new copy
            try { 
                std::vector< Record > freshCopy( m_slots );
                m_slots.swap( freshCopy );
            }
            catch (std::bad_alloc & ) {
                LOG(2, "Unable to allocate temporary memory to shrink memory register");
               /* ignore because we have the result already  */
            }
            
            try { 
                std::vector< Slot > freshCopy( m_free );
                freshCopy.reserve( newSize );
                m_free.swap( freshCopy );
            }
            catch (std::bad_alloc & ) {
                LOG(2, "Unable to allocate temporary memory to shrink memory register");
               /* ignore because we have the result already */
            }
        }
    }

private:
    // The slots themselves
    std::vector< Record > m_slots;
    
    // A stack of slots that can be recycled
    std::vector< Slot >   m_free;

    // the smallest slot number that hasn't been dealt out yet
    Slot                  m_minNewSlotNr; 
}; 

template <class Record>
class CombinedMemoryRegister
{
public:
    typedef typename MemoryRegister<Record>::Slot Slot;

    static Slot invalidSlot() { return MemoryRegister<Record>::invalidSlot() ; }

    explicit CombinedMemoryRegister()
        : m_local()
        , m_global()
    {}

    void destroy() {
        m_local.destroy();
        m_global.destroy();
        m_noc.destroy();
    }

    Slot addLocalReg( Record record )  // nothrow
    { 
        return toLocal( m_local.add( record ) ); 
    } 

    Slot addNocReg( Record record )  // nothrow
    { 
        return toNoc( m_noc.add( record ) ); 
    } 

    Slot addGlobalReg( Record record ) // nothrow
    { 
        return toGlobal( m_global.add(record) ); 
    }

    void removeReg( Slot slot )   // nothrow
    {
        if (isLocalSlot(slot))
            m_local.remove( fromLocal(slot) ) ;
        else if (isGlobalSlot(slot))
            m_global.remove( fromGlobal( slot ) );
        else 
            m_noc.remove(fromNoc( slot ) );
     }

    const Record & lookup( Slot slot ) const // nothrow
    {
        if (isLocalSlot(slot))
            return m_local.lookup( fromLocal(slot));
        else if (isGlobalSlot(slot))
            return m_global.lookup( fromGlobal( slot ));
        else {// isNocSlot(slot) == true
            printf("THIS IS A NOC SLOT!\n");
            return m_noc.lookup( fromNoc( slot ));
        }
    }

    Record & update( Slot slot ) // nothrow
    {
        if (isLocalSlot(slot))
            return m_local.update( fromLocal(slot));
        else
            return m_global.update( fromGlobal( slot ));
    }

    void reserve( size_t size, const Record & defaultRecord = Record() )
        // throws bad_alloc, strong safe
    {
        m_global.reserve( size, defaultRecord );
        m_local.reserve( size, defaultRecord );
        m_noc.reserve( size, defaultRecord );
    }

    size_t capacity( ) const
    { 
        return std::min(std::min( m_global.capacity(), m_local.capacity()), m_noc.capacity() );
    }

    size_t range() const
    {
        return std::max(std::max( 3*m_global.capacity(), 3*m_local.capacity()+1), 3*m_noc.capacity()+2);
    }

    static bool isLocalSlot( Slot slot ) 
    { return slot % 3 == 1; }

    static bool isGlobalSlot( Slot slot ) 
    { return slot % 3 == 0; }

    static bool isNocSlot( Slot slot ) 
    { return slot % 3 == 2; }

private:
    static Slot fromGlobal( Slot slot )
    { return slot / 3; }

    static Slot fromLocal( Slot slot )
    { return (slot - 1) / 3; }

    static Slot fromNoc( Slot slot )
    { return (slot - 2) / 3; }

    static Slot toGlobal( Slot slot )
    { return 3*slot; }

    static Slot toLocal( Slot slot )
    { return 3*slot + 1; }

    static Slot toNoc( Slot slot )
    { return 3*slot + 2; }

    MemoryRegister<Record> m_local;
    MemoryRegister<Record> m_global;
    MemoryRegister<Record> m_noc;
};

} // namespace lpf

#endif
