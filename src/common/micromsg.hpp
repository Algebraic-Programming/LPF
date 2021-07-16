
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

#ifndef PLATFORM_CORE_MPI_MICROMSG_HPP
#define PLATFORM_CORE_MPI_MICROMSG_HPP

#include <typeinfo>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>

#include "assert.hpp"
#include "linkage.hpp"

namespace lpf {

class _LPFLIB_LOCAL MicroMsg {
public:
    explicit MicroMsg( void * buf, size_t pos )
        : m_buffer( static_cast<unsigned char *>(buf) )
        , m_pos( pos )
    {
    }

    size_t pos() const
    { return m_pos; }

    template <class UInteger>
    const MicroMsg & write(UInteger number ) const
    {
        ASSERT( number >= 0 );
        const unsigned char hasNextBlock = 1u << (CHAR_BIT-1);
        const unsigned char block = hasNextBlock - 1;
        do
        {
            unsigned char c = (number & block);
            number >>= (CHAR_BIT-1);
            if ( number > 0 )
                c |= hasNextBlock;
            m_buffer[ m_pos++ ] = c ;
        }
        while (number > 0 );

        return *this;
    }
 
    const MicroMsg & write(const void * bytes, size_t size ) const
    {
        std::memcpy( m_buffer + m_pos, bytes, size );
        m_pos += size;
        return *this;
    }
       

    template <class UInteger>
    const MicroMsg & read(UInteger & result ) const
    {
        const unsigned char hasNextBlock = 1u << (CHAR_BIT-1);
        const unsigned char block = hasNextBlock - 1;
        size_t begin = m_pos;
        while ( m_buffer[ m_pos ] & hasNextBlock  )
        {
            m_pos++;
        }
        m_pos++;

        result = 0;
        for (size_t i = m_pos ; i > begin; --i ) {
            unsigned char c = m_buffer[ i-1 ] ; 
            result <<= (CHAR_BIT-1);
            result += (c & block );
        }

        return *this;
    }

    const MicroMsg & read(void * bytes, size_t size ) const
    {
        std::memcpy( bytes, m_buffer + m_pos, size );
        m_pos += size;
        return *this;
    }

    template <class UInteger>
    const MicroMsg & access( UInteger & x, bool rd  ) const
    { return rd?read(x):write(x); } 

    const MicroMsg & access( void * bytes, size_t size, bool rd  ) const
    { return rd?read(bytes,size):write(bytes,size); } 

private:
    unsigned char * m_buffer;
    mutable size_t m_pos;
};


}  //namespace lpf

#endif
