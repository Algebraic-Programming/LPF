
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

#ifndef LPF_CORE_MPI_MESSAGESORT_HPP
#define LPF_CORE_MPI_MESSAGESORT_HPP

#include <set>
#include <vector>

#include "sparseset.hpp"
#include "types.hpp"
#include "linkage.hpp"

namespace lpf {

class _LPFLIB_LOCAL MessageSort
{
public:
    typedef unsigned MsgId;
    static const MsgId MAX_ID=(1u<<31);

    MessageSort();

    void setSlotRange( memslot_t range );
    void addRegister( memslot_t id, char * base, size_t size);
    void delRegister( memslot_t id );
    
    // note: msgId is also a priority, zero being the lowest
    void pushWrite( MsgId msgId, memslot_t slot, size_t & offset, size_t & size );
    bool popWrite( MsgId & msgId, char * & base, size_t & size );
    bool canWrite( MsgId msgId, memslot_t slot, size_t offset ) const;
    
    void clear();
    bool empty() const;

private:
    struct Slot { memslot_t slot; char * base; size_t size; };
    
    struct Region { size_t region; char * base; size_t size; };
    struct CompareRegion { 
        bool operator()( const Region & a, const Region & b) const
        { return a.base + a.size < b.base; } 
    };
    typedef std::multiset< Region, CompareRegion > SortedRegions;
    typedef SortedRegions :: iterator SortedRegionsIt;
    typedef std::pair< SortedRegionsIt, SortedRegionsIt > RegionRange;

    struct SlotToRegion { size_t region; size_t offset ; };

    struct Writes { 
        size_t m_min, m_max;
        std::vector< MsgId > m_memory;

        Writes( size_t size ) ;
        Writes();

        void resize(size_t newSize);
        void clear();
        void push(MsgId id, size_t start, size_t size);
        bool pop( MsgId & id, size_t & start, size_t & size );
        bool test(MsgId id, size_t start ) const;
        void swap( Writes & other );
    };

    std::vector< Slot > m_memslots;
    std::vector< Region > m_regions;
    SortedRegions m_sortedRegions;
    std::vector< SlotToRegion > m_slotToRegion;
    std::vector< Writes > m_writes;
    SparseSet< memslot_t > m_dirty;
    const int m_msgGrainPower;
};



} 

#endif

