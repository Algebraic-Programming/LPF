
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

#include "messagesort.hpp"
#include "config.hpp"
#include <algorithm>

namespace lpf {

const MessageSort::MsgId MessageSort::MAX_ID;

MessageSort :: MessageSort()
    : m_memslots()
    , m_regions()
    , m_sortedRegions()
    , m_slotToRegion()
    , m_writes()
    , m_dirty(0, 0)
    , m_msgGrainPower( Config::instance().getMpiMsgSortGrainSizePower() )
{}

void MessageSort :: setSlotRange( memslot_t range )
{
    m_memslots.resize( range );
    m_regions.reserve( range );
    m_slotToRegion.resize( range );
    //FIXME allocate m_sortedSlots
    //FIXME preallocate write space
    //FIXME: allocation of m_dirty
}

void MessageSort :: addRegister( memslot_t id, char * base, size_t size)
{   
    //FIXME: allocation of m_dirty: current problem is that m_regions
    // can grow unboundedly
    while  ( m_regions.size() > m_dirty.range().second ) {
        size_t newMax = std::max(size_t(1), 2*m_dirty.range().second );
        m_dirty.resize( newMax );
    }

    Region newRegion = { m_regions.size(), base, size } ;
    
    // find regions that overlap with this new slot
    RegionRange range = m_sortedRegions.equal_range( newRegion );

    // get the entire extent of this "messy" (=overlapping) region
    char * minBase = newRegion.base;
    char * maxAddr = newRegion.base + newRegion.size;
    size_t minRegion = newRegion.region;
    typedef SortedRegionsIt It;
    for (It i = range.first; i != range.second; ++i )
    {
        if ( i->base < minBase ) {
            minBase = i->base;
        }

        if ( i->base + i->size > maxAddr ) {
            maxAddr = i->base + i->size;
        }

        if ( i->region < minRegion ) {
            minRegion = i->region;
        }
    }
    newRegion.base = minBase;
    newRegion.size = maxAddr - minBase;
    newRegion.region = minRegion;

    if ( range.first == range.second )
    {   // if there was no overlap, add the new region (is the same as the slot)
        ASSERT( newRegion.region == m_regions.size() );
        Writes empty;
        m_writes.push_back( empty );
        Writes array( newRegion.size >> m_msgGrainPower );
        m_writes.back().swap( array );
        m_regions.push_back( newRegion );
    }
    else
    {   // otherwise extend the oldest region of the overlapping area
        m_writes[ newRegion.region ].resize( newRegion.size >> m_msgGrainPower );
        m_regions[ newRegion.region ] = newRegion;
    }
   
    Slot slot = { id, base, size };
    ASSERT( id < m_memslots.size() );
    m_memslots[ id ] = slot;

    // return write space to memory heap from discarded regions
    for (It i = range.first; i != range.second; ++i)
    {
        if (i->region != newRegion.region) {
            Writes empty;
            m_writes[i->region].swap( empty );
        }
    }

    // Replace the old overlapping region with the new region.
    if ( range.first != range.second ) {
        for ( size_t s = 0; s < m_slotToRegion.size(); ++s )
        {
            for (It i = range.first; i != range.second; ++i)
                if (m_slotToRegion[s].region == i->region) {
                    m_slotToRegion[s].region = newRegion.region;
                    m_slotToRegion[s].offset 
                        = m_memslots[s].base - newRegion.base;
                }
        }
    }
    m_sortedRegions.erase( range.first, range.second );
    (void) m_sortedRegions.insert( newRegion );
    m_slotToRegion[ id ].region = newRegion.region;
    m_slotToRegion[ id ].offset = slot.base - newRegion.base ;
}

void MessageSort :: delRegister( memslot_t id )
{
    // remove any reference to the old memory slot. Recalculating
    // the remaining regions and adjusting m_writes seems to be
    // overengineering. The main reason to do it nevertheless would be to
    // save memory, but it seems that not much will saved when we do try.
    const Slot null = { memslot_t(-1) , NULL, 0 };
    m_memslots[ id ] = null;
    m_slotToRegion[ id ].region = size_t(-1);
    m_slotToRegion[ id ].offset = size_t(-1);
}


void MessageSort :: pushWrite( MsgId msgId,
        memslot_t slot, size_t & offset, size_t & size )
{
    if (size == 0 ) return;
    size_t region = m_slotToRegion[slot].region;
    size_t start = m_slotToRegion[slot].offset + offset; 
    // diff = (MSG_GRAIN - 1 - (start + MSG_GRAIN - 1) % MSG_GRAIN)
    size_t msgGrainSizeM1 = (1ul << m_msgGrainPower) - 1;
    size_t diff = (msgGrainSizeM1 - ((start + msgGrainSizeM1) & msgGrainSizeM1) );
    //ASSERT( (start + diff ) % MSG_GRAIN == 0);
    ASSERT( ((start + diff) & msgGrainSizeM1) == 0);
    m_writes[ region ].push(msgId, 
            (start + diff) >> m_msgGrainPower,
            (size - diff) >> m_msgGrainPower );
    m_dirty.insert( region );

    offset += diff;
    size = (size - diff ) & (~msgGrainSizeM1); // align w.r.t MSG_GRAIN
}

bool MessageSort :: popWrite( MsgId & msgId, char * & base, size_t & size )
{
    while ( !m_dirty.empty() )
    {
        size_t region = *m_dirty.begin();
        size_t start = size_t(-1);

        if (m_writes[ region ].pop( msgId, start, size )) {
            base = m_regions[region].base + (start << m_msgGrainPower);
            size <<= m_msgGrainPower;
            return true;
        }

        m_dirty.erase( region );
    }

    return false;
}

bool MessageSort :: canWrite( MsgId msgId, memslot_t slot, size_t offset ) const
{
    size_t region = m_slotToRegion[slot].region;
    size_t start = m_slotToRegion[slot].offset + offset; 
    ASSERT( region < m_writes.size() );
    return m_writes[ region ].test(msgId, start >> m_msgGrainPower );
}

void MessageSort :: clear()
{
    m_dirty.clear();
    for (size_t i = 0; i < m_writes.size(); ++i)
        m_writes[i].clear();
}

bool MessageSort :: empty() const
{
    return m_dirty.empty();
}

MessageSort :: Writes :: Writes( size_t size ) 
    : m_min(-1), m_max(0)
    , m_memory(size)
{}

MessageSort :: Writes :: Writes()
    : m_min(-1), m_max(0)
    , m_memory() 
{}

void MessageSort :: Writes :: resize(size_t newSize) 
{
    ASSERT( newSize >= m_memory.size() );
    m_memory.resize( newSize);
}

void MessageSort :: Writes :: clear()
{
    if (m_min < m_max ) {
        ASSERT( m_max + 1 <= m_memory.size() );
        ASSERT( m_min <= m_memory.size() );
        std::fill( m_memory.begin() + m_min,
               m_memory.begin() + m_max + 1, 0u);
    }

    m_min = size_t(-1);
    m_max = 0;
}

void MessageSort :: Writes :: push(MsgId id, size_t start, size_t size)
{
    ASSERT( start + size <= m_memory.size() );
    for (size_t i = start; i < start + size; ++i)
        if ( id >= m_memory[i] )
            m_memory[i] = id + 1;

    m_max = std::max( m_max, start + size - 1 );
    m_min = std::min( m_min, start );
}

bool MessageSort :: Writes :: test(MsgId id, size_t start) const
{
    ASSERT( start <= m_memory.size() );
    return start == m_memory.size() || id >= m_memory[start];
}

bool MessageSort :: Writes :: pop( MsgId & id, size_t & start, size_t & size )
{
    if ( m_max < m_min ) return false;

    size_t i = m_min;
    id = m_memory[ i ] - 1;

    do
    {
        m_memory[ i ] = 0;
        i++;
    }
    while ( i <= m_max && m_memory[ i ] == id+1 );

    start = m_min;
    size = i - m_min;
    m_min = i;

    ASSERT( m_max < m_memory.size() );
    while (m_min <= m_max && m_memory[m_min] == 0 )
        m_min++;

    if ( m_min > m_max )
    {
        ASSERT( m_max == m_min -1 );
        m_min=size_t(-1);
        m_max=0;
    }

    return true;
}

void MessageSort :: Writes :: swap( Writes & other )
{
    using std::swap;
    swap( this->m_min, other.m_min );
    swap( this->m_max, other.m_max );
    m_memory.swap( other.m_memory );
}
            
}  // namespace lpf
