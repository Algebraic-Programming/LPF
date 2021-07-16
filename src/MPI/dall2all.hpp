
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

#ifndef LPF_CORE_MPI_DENSE_ALL2ALL_HPP
#define LPF_CORE_MPI_DENSE_ALL2ALL_HPP

#include "mpilib.hpp"
#include "assert.hpp"
#include "vall2all.hpp"
#include "linkage.hpp"

#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cstring>

namespace lpf { namespace mpi {


class _LPFLIB_LOCAL DenseAllToAll : public VirtualAllToAll
{
public:
    typedef ptrdiff_t   difference_type;
    typedef std::size_t size_type;

    DenseAllToAll(int pid, int nprocs, int nvotes = 0)
        : m_pid( pid )
        , m_nprocs( nprocs )
        , m_ballot( nvotes )
        , m_totalSend(0), m_totalRecv(0), m_recvIndex(0)
        , m_send(), m_sendSorted(), m_recvSorted()
        , m_sendCounts(nprocs)
        , m_recvCounts(nprocs)
        , m_sendOffsets(nprocs)
        , m_recvOffsets(nprocs)
    {   
    }

    size_type max_size( size_t maxMsgSize ) const
    { 
        return m_send.max_size() / (sizeof(Header) + maxMsgSize);
    }

    void reserve( size_type number, size_t maxMsgSize )
    {
        size_t n = number * (sizeof(Header) + maxMsgSize );

        if (n > unsigned(std::numeric_limits<int>::max()))
            throw std::bad_alloc();

        if ( m_send.size() < n
                || m_sendSorted.size() < n
                || m_recvSorted.size() < n )
        {
            m_send.resize( n );
            m_sendSorted.resize( n );
            m_recvSorted.resize( n );
        }
    }

    void clear()
    { 
       std::fill( m_sendCounts.begin(), m_sendCounts.end(), 0);
       std::fill( m_recvCounts.begin(), m_recvCounts.end(), 0);
       m_totalRecv = 0;
       m_totalSend = 0;
       m_recvIndex = 0;
    }

    bool empty() const
    {
        return m_totalSend == 0 && m_recvIndex == m_totalRecv;
    }

    void send( pid_t pid, const void * msg, size_t size)
    { 
        ASSERT( m_totalSend + size + sizeof(Header) <= m_send.size() );
        char * ptr = m_send.data() + m_totalSend;
        m_sendCounts[pid] += size + sizeof(Header);
        m_totalSend += size + sizeof(Header);

        Header header = { int(size), pid };
        std::memcpy( ptr, &header, sizeof(header));
        ptr += sizeof(header);
        std::memcpy( ptr, msg, size );
    }

    size_t recv( void * buf, size_t maxSize )
    {

        char * ptr = m_recvSorted.data() + m_recvIndex;
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

        // exchange volumes
        comm.allToAll( m_sendCounts.data(), m_recvCounts.data(),
                sizeof(int) );

        // compute receive and send offsets 
        int recvOffset = 0, sendOffset = 0;
        for (pid_t p = 0; p < m_nprocs; ++p) {
           m_recvOffsets[p] = recvOffset;
           recvOffset += m_recvCounts[p];

           m_sendOffsets[p]=sendOffset ;
           sendOffset += m_sendCounts[p];
        }
        m_totalRecv = recvOffset;
        ASSERT( unsigned(sendOffset) == m_totalSend );

        // sort send data per destination
        for (unsigned n = 0; n < m_totalSend;  ) {
            char * ptr = m_send.data() + n;
            Header header;
            std::memcpy( &header, ptr, sizeof(header));

            std::memcpy( m_sendSorted.data() + m_sendOffsets[header.pid],
                    ptr, header.size + sizeof(header));

            m_sendOffsets[header.pid] += header.size + sizeof(header);
            n += sizeof(header) + header.size;
        }

        // compute  send offsets again
        sendOffset = 0;
        for (pid_t p = 0; p < m_nprocs; ++p) {
           m_sendOffsets[p]=sendOffset ;
           sendOffset += m_sendCounts[p];
        }

        // Do the all-to-all-v
        comm.allToAll( m_sendSorted.data(), 
                m_sendOffsets.data(), m_sendCounts.data(),
                m_recvSorted.data(), m_recvOffsets.data(), 
                m_recvCounts.data() );


        // set index
        std::fill( m_sendCounts.begin(), m_sendCounts.end(), 0);
        m_totalSend = 0;
        m_recvIndex = 0;

        comm.allreduceSum( vote, m_ballot.data(), m_ballot.size() );

        std::copy( m_ballot.begin(), m_ballot.end(), vote );
        return 0; 
    }

private:
    struct Header { int size; pid_t pid; }; 

    pid_t m_pid;
    pid_t m_nprocs;
    std::vector< int > m_ballot;
    unsigned m_totalSend, m_totalRecv;
    unsigned m_recvIndex;
    std::vector< char > m_send;
    std::vector< char > m_sendSorted, m_recvSorted;
    std::vector< int > m_sendCounts, m_recvCounts;
    std::vector< int > m_sendOffsets, m_recvOffsets;
};


} } // namespaces lpf, mpi

#endif
