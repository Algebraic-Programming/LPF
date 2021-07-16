
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

#ifndef LPF_CORE_MPI_HOEFLER_ALL2ALL_HPP
#define LPF_CORE_MPI_HOEFLER_ALL2ALL_HPP

#include "mpilib.hpp"
#include "assert.hpp"
#include "vall2all.hpp"
#include "linkage.hpp"

#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cstring>

namespace lpf { namespace mpi {


class _LPFLIB_LOCAL HoeflerAllToAll: public VirtualAllToAll
{
public:
    typedef ptrdiff_t   difference_type;
    typedef std::size_t size_type;

    HoeflerAllToAll(int pid, int nprocs, int nvotes=0)
        : m_pid( pid )
        , m_nprocs( nprocs )
        , m_ballot(nvotes)
        , m_totalRecv(0), m_recvIndex(0)
        , m_send(), m_recv()
        , m_sendSorted()
        , m_sendCounts()
        , m_sendOffsets()
        , m_sendPids()
        , m_tempSpace()
    {   
    }

    size_t max_size( size_t maxMsgSize ) const
    {
        return m_send.max_size() / (sizeof(Header) + maxMsgSize);
    }

    void reserve( size_type number, size_t maxMsgSize )
    {
#ifndef MPI_HAS_IBARRIER        
        (void) number;
        (void) maxMsgSize;
        throw std::bad_alloc();
#else
        size_t n = number * (sizeof(Header) + maxMsgSize );
        if ( m_send.capacity() < n)
        {
            m_send.reserve( n );
            m_sendSorted.reserve( n );
            m_recv.resize( n );

            size_t m = std::min< size_t >( m_nprocs, number);
            m_sendCounts.resize( m );
            m_sendOffsets.resize( m );
            m_sendPids.resize(m);
            
            Comm::allToAllAllocTempspace( m, m_tempSpace );
        }
#endif
    }

    void clear()
    { 
        m_send.clear();
        m_sendSorted.clear();
        m_totalRecv = 0;
        m_recvIndex = 0;
    }

    bool empty() const
    {
        return m_send.empty() && m_recvIndex == m_totalRecv;
    }

    void send( pid_t pid, const void * msg, size_t size)
    { 
        Header header = { size, pid };
        char h[sizeof(header)];
        std::memcpy( h, &header, sizeof(header));
        const char * m = static_cast<const char *>(msg);

        m_send.insert( m_send.end(), h, h+sizeof(header));
        m_send.insert( m_send.end(), m, m+size);
    }

    size_t recv( void * buf, size_t maxSize )
    {
        char * ptr = m_recv.data() + m_recvIndex;
        Header header;

        std::memcpy( &header, ptr, sizeof(header));
        ASSERT( m_recvIndex + header.size + sizeof(Header) <= m_totalRecv );

        ptr += sizeof(header);

        size_t size = header.size;
        if ( maxSize < size )
            size = maxSize;
        std::memcpy( buf, ptr, size );

        m_recvIndex += sizeof(header) + header.size;

        return header.size; 
    }


    int exchange( const mpi::Comm & comm, bool prerandomize,
           int * vote, int retry=5 )
    { 
        (void) prerandomize; // unused parameter 
        (void) retry; // unused parameter
#ifdef MPI_HAS_IBARRIER        

        int messages = -1;
        if ( m_sendCounts.size() == size_t(m_nprocs) ) {
            messages = m_nprocs;
            // we have enough space to sort the messages
            // compute receive and send offsets 
            
            // first count the data volume per destination process

            std::fill( m_sendCounts.begin(), m_sendCounts.end(), 0);
            for (size_t n = 0; n < m_send.size();  ) {
                char * ptr = m_send.data() + n;
                Header header;
                std::memcpy( &header, ptr, sizeof(header));

                m_sendCounts[header.pid] += header.size + sizeof(header);
                n += sizeof(header) + header.size;
            }
            
            // compute what the offsets would be
            size_t sendOffset = 0;
            for (pid_t p = 0; p < m_nprocs; ++p) {
               m_sendOffsets[p]=sendOffset ;
               sendOffset += m_sendCounts[p];
            }
            ASSERT( sendOffset == m_send.size() );

            // then put the messages in order
            Header header;
            for (size_t n = 0; n < m_send.size();
                    n += sizeof(header) + header.size ) {
                char * ptr = m_send.data() + n;
                std::memcpy( &header, ptr, sizeof(header));

                std::memcpy( m_sendSorted.data() + m_sendOffsets[header.pid],
                        ptr, header.size + sizeof(header));

                m_sendOffsets[header.pid] += header.size + sizeof(header);
            }

            // compute the send offsets once more
            sendOffset = 0;
            for (pid_t p = 0; p < m_nprocs; ++p) {
               m_sendPids[p] = p;
               m_sendOffsets[p]=sendOffset ;
               sendOffset += m_sendCounts[p];
            }
            m_send.swap( m_sendSorted );

        }
        else // we can treat each message separately
        {
            std::fill( m_sendCounts.begin(), m_sendCounts.end(), 0);
            std::fill( m_sendOffsets.begin(), m_sendOffsets.end(), 0);
            unsigned i = 0;
            Header header;
            for (size_t n = 0; n < m_send.size(); 
                    n += sizeof(header) + header.size ) {
                char * ptr = m_send.data() + n;
                std::memcpy( &header, ptr, sizeof(header));

                m_sendOffsets[i] = n;
                m_sendCounts[i] = header.size + sizeof(header);
                m_sendPids[i] = header.pid;
                i += 1;
            }
            messages = i;
        }

        // Do the all-to-all-v
        comm.allToAll( messages,
                m_send.data(), m_sendOffsets.data(), m_sendCounts.data(), m_sendPids.data(),
                m_recv.data(), m_recv.size(), &m_totalRecv, m_tempSpace );

        // clear
        m_send.clear();
        m_sendSorted.clear();
        m_recvIndex = 0;

#endif
        comm.allreduceSum( vote, m_ballot.data(), m_ballot.size() );

        std::copy( m_ballot.begin(), m_ballot.end(), vote );
        return 0;
    }

private:
    struct Header { size_t size; pid_t pid; }; 

    pid_t m_pid;
    pid_t m_nprocs;
    std::vector<int> m_ballot;
    size_t m_totalRecv;
    size_t m_recvIndex;
    std::vector< char > m_send, m_recv;
    std::vector< char > m_sendSorted;
    std::vector< size_t > m_sendCounts;
    std::vector< size_t > m_sendOffsets;
    std::vector< int > m_sendPids;
    std::vector< char > m_tempSpace;
};



} } // namespaces lpf, mpi

#endif
