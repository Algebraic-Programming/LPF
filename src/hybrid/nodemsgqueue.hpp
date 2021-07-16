
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

#ifndef LPFLIB_HYBRID_NODEMSGQUEUE_HPP
#define LPFLIB_HYBRID_NODEMSGQUEUE_HPP

#include "dispatch.hpp"
#include "assert.hpp"
#include "linkage.hpp"

#include <vector>
#include <algorithm>
#include <numeric>

namespace lpf { namespace hybrid {

class _LPFLIB_LOCAL NodeMsgQueue
{
public:
    struct Put { 
        lpf_memslot_t m_srcSlot, m_dstSlot;
        size_t m_srcOffset, m_dstOffset, m_size;
        MPI::pid_t m_dstPid;

        Put( lpf_memslot_t srcSlot, size_t srcOffset, MPI::pid_t dstPid,
                lpf_memslot_t dstSlot, size_t dstOffset, size_t size )
            : m_srcSlot( srcSlot), m_dstSlot( dstSlot )
            , m_srcOffset( srcOffset ) , m_dstOffset( dstOffset )
            , m_size( size), m_dstPid( dstPid )  {}
    };

    struct Get { 
        lpf_memslot_t m_srcSlot, m_dstSlot;
        size_t m_srcOffset, m_dstOffset, m_size;
        MPI::pid_t m_srcPid;
    
        Get( MPI::pid_t srcPid, lpf_memslot_t srcSlot, size_t srcOffset,
                lpf_memslot_t dstSlot, size_t dstOffset, size_t size )
            : m_srcSlot( srcSlot), m_dstSlot( dstSlot )
            , m_srcOffset( srcOffset ) , m_dstOffset( dstOffset )
            , m_size( size), m_srcPid( srcPid )  {}
    };

    NodeMsgQueue( MPI::pid_t nodeId, const std::vector< MPI::pid_t > & threadCounts )
        : m_globalToNode( std::accumulate(threadCounts.begin(), threadCounts.end(), 0) )
        , m_globalToThread( m_globalToNode.size() )
        , m_puts( threadCounts[nodeId] )
        , m_gets( threadCounts[nodeId] )
    {
        // set global-to-node array
        std::vector< MPI::pid_t > :: iterator a = m_globalToNode.begin();
        std::vector< MPI::pid_t > :: iterator b = a;
        for (size_t i = 0; i < threadCounts.size(); ++i )
        {
            a = b;
            b += threadCounts[i];
            std::fill( a, b, MPI::pid_t(i) );
        }

        // set global-id-to-thead-id array
        for (size_t i = 0, k = 0; i < threadCounts.size(); ++i )
        {
            for (size_t j = 0; j < threadCounts[i]; ++j, ++k)
                m_globalToThread[k] = j;
        }
    }

    void push( size_t thread, const Put & put)
    { 
        m_puts[thread].push_back( put );
    }

    void push( size_t thread, const Get & get)
    {
        m_gets[thread].push_back( get );
    }

    void reserve( size_t thread, size_t nMsgs )
    {
        ASSERT( m_puts.size() == m_gets.size() );
        m_puts[thread].reserve( nMsgs );
        m_gets[thread].reserve( nMsgs );
    }

    void flush( MPI mpi, const NodeMemReg & memreg )
    {
        typedef NodeMemReg::Memory Memory;
        ASSERT( m_puts.size() == m_gets.size() );
        
        MPI::err_t nrc = MPI::SUCCESS;
       
        for (size_t localThread = 0; localThread < m_puts.size(); ++localThread)
        {
            for (size_t i = 0; i < m_gets[localThread].size(); ++i)
            {
                const Get & g = m_gets[localThread][i];
                size_t srcThread = m_globalToThread[g.m_srcPid ];
                const Memory & src = memreg.lookup( srcThread, g.m_srcSlot );
                const Memory & dst = memreg.lookup( localThread, g.m_dstSlot );
                MPI::pid_t srcNodePid = m_globalToNode[ g.m_srcPid ];
                nrc = mpi.get( srcNodePid, src.m_nodeSlot, g.m_srcOffset,
                        dst.m_nodeSlot, g.m_dstOffset, g.m_size );
                ASSERT( MPI::SUCCESS == nrc );
            }
            m_gets[localThread].clear();
            for (size_t i = 0; i < m_puts[localThread].size(); ++i) 
            {
                const Put & p = m_puts[localThread][i];
                size_t dstThread = m_globalToThread[p.m_dstPid];
                const Memory & src = memreg.lookup( localThread, p.m_srcSlot );
                const Memory & dst = memreg.lookup( dstThread, p.m_dstSlot );
                MPI::pid_t dstNodePid = m_globalToNode[ p.m_dstPid ];
                nrc = mpi.put( src.m_nodeSlot, p.m_srcOffset,
                        dstNodePid, dst.m_nodeSlot, p.m_dstOffset, p.m_size );
                ASSERT( MPI::SUCCESS == nrc );
            }
            m_puts[localThread].clear();
        }
    }


private:
    std::vector< MPI::pid_t > m_globalToNode;
    std::vector< Thread::pid_t > m_globalToThread;
    std::vector< std::vector< Put > > m_puts;
    std::vector< std::vector< Get > > m_gets;
};



} }


#endif
