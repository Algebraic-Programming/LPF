
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

#ifndef PLATFORM_CORE_MSGQUEUE_HPP
#define PLATFORM_CORE_MSGQUEUE_HPP

#include <vector>
#include <utility>
#include <algorithm>
#include <cstring>

#include "types.hpp"
#include <memreg.hpp>
#include <assert.hpp>
#include <log.hpp>
#include <micromsg.hpp>
#include <linkage.hpp>

namespace lpf {

class _LPFLIB_LOCAL MsgQueue
{
    typedef size_t Pointer ;

    /** A data-structure that points to the last element in the queue for a
     * specific destination processor. 
     * The last element, which doesn't match a PID, states the total amount
     * of messages destined for that processor*/
    typedef std::vector< Pointer > Index;

    static size_t accessHeader( pid_t & srcPid, memslot_t & dstSlot, size_t & dstOffset, size_t & size, 
            Pointer & previous, void * buffer, size_t pos, bool read )
    {
        return MicroMsg( buffer, pos)
            . access( srcPid, read )
            . access( dstSlot, read )
            . access( dstOffset, read )
            . access( size, read )
            . access( previous, read )
            . pos();
    }
    static size_t accessSmallBody( void * payload, size_t size, 
                                   void * buffer, size_t pos, bool read )
    {
        return MicroMsg( buffer, pos)
            . access( payload, size, read )
            . pos();
    }
    static size_t accessLargeBody( memslot_t & srcSlot, size_t & srcOffset,
                                   void * buffer, size_t pos, bool read )
    {
        return MicroMsg( buffer, pos)
            . access( srcSlot, read )
            . access( srcOffset, read )
            . pos();
    }

    static size_t getMaxHdrSize( pid_t nprocs, size_t memRange, size_t memslots, size_t msgs ) ;

    /** A data-structure to hold all messages for all destination processes.
     * Messages destined for the same processor are stored in a linked-list.
     * The first item of the pair points to the next message for the same
     * processor. */
    typedef std::vector< char > Queue;

    /** To mark the last message in the list, this value is used as pointer. */
    static const Pointer NotAvailable = static_cast<Pointer>(-1);

public:
    explicit MsgQueue(pid_t nprocs ); // throws std::bad_alloc

    void init(pid_t myPid) ; // throws std::bad_alloc
    void destroy(pid_t myPid) ;
    void clear(pid_t myPid);  // nothrow
    void reserve( pid_t myPid, size_t memslots,
            size_t msgs, bool allowToShrink ); // bad_alloc, strong exception safe


    template <typename Register>
    void push( pid_t myPid, pid_t srcPid,  memslot_t srcSlot, size_t srcOffset,
            pid_t destPid, memslot_t dstSlot, size_t dstOffset,
            size_t size, const Register & reg ) // nothrow
    {
        ASSERT( myPid < m_index.size() );
        ASSERT( myPid < m_msgs.size() );
        ASSERT( m_index[myPid].back() < m_msgs[myPid].size() );

        // get a pointer to the top of the message stack
        Pointer & top = m_index[myPid].back();

        // get a pointer to the previous element of the message queue to
        // processor 'destPid'
        Pointer & previous = m_index[myPid][destPid];

        // add the message to the message queue with a link to the previous
        // element
        size_t newTop = accessHeader( srcPid, dstSlot, dstOffset, size, previous,
                m_msgs[myPid].data(), top, false );
        if ( size <= m_tinyMsgSize && srcPid == myPid ) {
            void * payload = reg[myPid].lookup(srcSlot).addr + srcOffset ;
            newTop = accessSmallBody( payload, size,
                                  m_msgs[myPid].data(), newTop, false );
        }
        else {
            newTop = accessLargeBody( srcSlot, srcOffset,
                                   m_msgs[myPid].data(), newTop, false );
        }


        // store the position of this last element
        previous = top;

        // increase the top of the stack
        top = newTop;
    }

    template <typename Register >
    void execute(pid_t myPid, const Register & reg ) // nothrow
    {
        LPFLIB_IGNORE_SHORTEN
        const pid_t nprocs = m_index.size();
        LPFLIB_RESTORE_WARNINGS
        // For each process that has communication for me:
        for ( pid_t remotePid = 0; remotePid < nprocs; ++remotePid)
        {
            // process its message queue
            Pointer previous = NotAvailable;
            for( Pointer i = m_index[remotePid][myPid]; i != NotAvailable; i = previous )
            {  
                
                size_t size;
                pid_t srcPid;
                memslot_t dstSlot;
                size_t dstOffset;
                
                size_t j = accessHeader( srcPid, dstSlot, dstOffset, size, previous,
                                         m_msgs[remotePid].data(), i, true);

                char * dst = reg[myPid].lookup( dstSlot).addr + dstOffset ;

                if ( size <= m_tinyMsgSize && srcPid == remotePid ) {
                    (void) accessSmallBody( dst, size, m_msgs[remotePid].data(), j, true );
                }
                else {
                    memslot_t srcSlot;
                    size_t srcOffset;
                    (void) accessLargeBody( srcSlot, srcOffset,
                                       m_msgs[remotePid].data(), j, true );


#ifndef NDEBUG
                    ASSERT(srcOffset + size <= reg[srcPid].lookup(srcSlot).size);
                    ASSERT(dstOffset + size <= reg[myPid].lookup(dstSlot).size);
#endif
                    char * src = reg[srcPid].lookup(srcSlot).addr + srcOffset ;
                    ASSERT( src + size <= dst || dst + size <= src);
                    std::memcpy( dst, src, size );
                }
            }
        }
#if 0
        fprintf(stderr, "[%02u] volume = %zu\n", myPid, volume);
#endif
    }

    void swap( MsgQueue & other ) // nothrow
    {
        this->m_index.swap( other.m_index );
        this->m_msgs.swap( other.m_msgs );
    }

private:
    std::vector< Index >   m_index;
    std::vector< Queue >   m_msgs;
    const size_t           m_tinyMsgSize;
    const size_t           m_maxMemRange;
};



} // namespace lpf
#endif
