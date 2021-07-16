
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

#include "msgqueue.hpp"
#include "config.hpp"

#include <cmath>

namespace lpf {

const MsgQueue::Pointer MsgQueue::NotAvailable;

MsgQueue::MsgQueue(pid_t nprocs ) // throws std::bad_alloc
    : m_index(nprocs)
    , m_msgs(nprocs)
    , m_tinyMsgSize( Config::instance().getTinyMsgSize().sizeForSharedMem )
    , m_maxMemRange( Config::instance().getLocalRamSize() )
{
}

size_t MsgQueue::getMaxHdrSize( pid_t nprocs, size_t memRange, size_t memslots, size_t msgs ) 
{
    // overestimate the header size
    size_t SupHdrSize
        = sizeof(pid_t) // srcPid
      + 2 * sizeof(memslot_t) // srcSlot + dstSlot
      + 3 * sizeof(size_t)  // srcOffset + dstOffset + size
      + sizeof(Pointer)  // previous
      + 2* 7 // at most 2 bytes of compression overhead per entry 
      ; 

    using std::ceil;
    using std::max;

    size_t size = static_cast<size_t>(ceil( max( 1.0, log2(nprocs) / 7.0 )) // srcPid
        +  2* ceil( max( 1.0, log2(1+memslots) / 7.0 )) // srcSlot + dstSlot
        +  3* ceil( max( 1.0, log2(1+memRange) / 7.0 )) // srcOffset + dstOffset + size
        +  ceil( max( 1.0, log2(1+ SupHdrSize * msgs ) / 7.0 )) // previous
        );

    LOG( 3, "The largest header size in pthreads engine is " 
            << size << " bytes, because nprocs = " << nprocs 
            << ", memory range = " << memRange 
            << ", max registers = " << memslots 
            << ", and max messages = " << msgs );

    return size;
}

void MsgQueue::init(pid_t myPid) // throws std::bad_alloc
{
    LPFLIB_IGNORE_SHORTEN
    const pid_t nprocs = m_index.size();
    LPFLIB_RESTORE_WARNINGS
    ASSERT( myPid < nprocs );

    if (nprocs > m_index[myPid].max_size() - 1)
        throw std::bad_alloc();

    m_index[myPid].resize( nprocs + 1, NotAvailable );
    m_index[myPid].back() = 0;
}

void MsgQueue::destroy(pid_t myPid)
{
    LPFLIB_IGNORE_SHORTEN
    const pid_t nprocs = m_index.size();
    LPFLIB_RESTORE_WARNINGS
    ASSERT( myPid < nprocs );
    m_index[myPid].clear();
    m_msgs [myPid].clear();
}

void MsgQueue::clear(pid_t myPid) // nothrow
{
    const Pointer NA = NotAvailable;
    ASSERT( myPid < m_index.size() );
    std::fill( m_index[myPid].begin(), m_index[myPid].end(), NA );
    m_index[myPid].back() = 0;
}

void MsgQueue :: reserve( pid_t myPid, size_t memslots, size_t msgs,
        bool allowToShrink ) // bad_alloc, strong exception safe
{
    ASSERT( myPid < m_msgs.size() );
    LPFLIB_IGNORE_SHORTEN
    const pid_t nprocs = m_msgs.size();
    LPFLIB_RESTORE_WARNINGS

    size_t nextHdrSize= getMaxHdrSize( nprocs, m_maxMemRange, memslots, msgs );
    size_t maxMsgSize = m_tinyMsgSize + nextHdrSize;
    if (msgs > m_msgs[myPid].max_size() / maxMsgSize )
        throw std::bad_alloc();

    if ( allowToShrink || msgs * maxMsgSize > m_msgs[myPid].size() )
    {
        m_msgs[myPid].resize( msgs * maxMsgSize );

        try {
            Queue newQueue( m_msgs[myPid] );
            m_msgs[myPid].swap( newQueue );
        }
        catch( std::bad_alloc & )
        {
            LOG(2, "Unable to allocate temporary memory to shrink memory register");
            /* ignore, because we have already the result we want */
        }
    }
}


}
