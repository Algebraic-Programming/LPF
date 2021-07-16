
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

#ifndef LPF_CORE_MPI_IPCMESG_HPP
#define LPF_CORE_MPI_IPCMESG_HPP

#include <typeinfo>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>

#include "assert.hpp"

namespace lpf { namespace mpi {

//lint -egrep( 534, IPCMesg<[^>]*>::read )   Return codes provide only syntactic sugar

template <class MsgEnum>
class IPCMesg {
public:
    explicit IPCMesg( char * buf, size_t size, size_t pos )
        : m_buffer( buf )
        , m_size( size )
        , m_pos( pos )
    {
#ifndef NDEBUG
        while (true) {
            ASSERT( m_pos < m_size );
            if (m_buffer[m_pos] == ';') { m_pos++; break; }
            m_pos ++;
        }
#endif
        m_pos += sizeof(MsgEnum );
        ASSERT( m_pos <= m_size );
    }

    static IPCMesg writeMsg( MsgEnum type, char * buf, size_t size )
    {
        ASSERT( sizeof(type) <= size );
        size_t pos = 0;
#ifndef NDEBUG
        pos = snprintf(buf, size, "%s;",typeid( MsgEnum ).name() );
#endif
        ASSERT( pos <= size - sizeof(type) );
        std::memcpy( buf + pos , &type, sizeof(type));

        return IPCMesg( buf, size, 0);
    }
 
    static IPCMesg readMsg( char * buf, size_t size )
    {
        return IPCMesg( buf, size, 0 );
    }
   
    MsgEnum type() const 
    {
        size_t pos = 0;
#ifndef NDEBUG
        char msgType[101];
        std::memset(msgType, 0, sizeof(msgType));
        int rc = std::sscanf( m_buffer, "%100[^;];", msgType );
        ASSERT( rc == 1);
        const char * trueTypeName = typeid(MsgEnum).name();
        ASSERT( std::strncmp( msgType, trueTypeName, 100 ) == 0 );
        while (true) {
            ASSERT( pos < m_size );
            if (m_buffer[pos] == ';') { pos++; break; }
            pos ++;
        }
#endif
        MsgEnum x;
        std::memcpy( &x, m_buffer + pos, sizeof(x));
        return x;
    }


    template <class Q>
    void send( Q & queue, pid_t dstPid ) const
    { 
        queue.send( dstPid, m_buffer, m_pos );
    }

    void rewind() 
    {
        *this = IPCMesg( m_buffer, m_size, 0);
    }

    size_t bytesLeft() const
    { 
        ASSERT( m_pos <= m_size );
        if ( m_pos == m_size )
            return 0;

        size_t pos = m_pos;
#ifndef NDEBUG
        int semicol = 0;
        while (semicol < 3 ) {
            ASSERT( pos < m_size );
            if (m_buffer[pos] == ';') semicol += 1;
            pos ++;
        }
#endif
        return m_size - pos; 
    }

    size_t pos() const
    { return m_pos; }

    template <class Enum, class UInteger>
    const IPCMesg & write(Enum id, UInteger number ) const
    {
        (void) id;
#ifndef NDEBUG
        ASSERT( number >= 0 );
        m_pos += snprintf(m_buffer + m_pos, m_size - m_pos, "%s;%s;%d;",
                typeid( Enum ).name(), typeid(number).name(), int(id) );
        ASSERT( m_pos <= m_size );
#endif
        const unsigned hasNextBlock = 1u << (CHAR_BIT-1);
        const unsigned block = hasNextBlock - 1;
        do
        {
            unsigned char c = (number & block);
            number >>= (CHAR_BIT-1);
            if ( number > 0 )
                c |= hasNextBlock;
            ASSERT( m_pos < m_size );
            m_buffer[ m_pos++ ] = char( c );
        }
        while (number > 0 );

        ASSERT( m_pos <= m_size );
        return *this;
    }
 
    template <class Enum>
    const IPCMesg & write(Enum id, bool number ) const
    { return write<Enum, unsigned char>( id, number?'\01':'\0'); }

    template <class Enum>
    const IPCMesg & write(Enum id, const void * bytes, size_t size ) const
    {
        (void) id;
#ifndef NDEBUG
        m_pos += snprintf(m_buffer + m_pos, m_size - m_pos, "blob;%d;;",int(id));
        ASSERT( m_pos <= m_size );
#endif
        ASSERT( size < m_size - m_pos );
        std::memcpy( m_buffer + m_pos, bytes, size );
        m_pos += size;
        
        ASSERT( m_pos <= m_size );
        return *this;
    }
       

    template <class Enum, class UInteger>
    const IPCMesg & read(Enum id, UInteger & result ) const
    {
        (void) id;
#ifndef NDEBUG
        char enumType[101];
        char numberType[101];
        int enumId = -1;
        std::memset(enumType, 0, sizeof(enumType));
        std::memset(numberType, 0, sizeof(numberType));

        int rc = std::sscanf( m_buffer + m_pos, "%100[^;];%100[^;];%d;",
                enumType, numberType, &enumId );
            ASSERT( rc == 3 );
            const char * trueEnumType = typeid(Enum).name();
            const char *  trueNumberType = typeid(UInteger).name();
            ASSERT( std::strncmp( enumType, trueEnumType, 100 ) == 0 );
            ASSERT( std::strncmp( numberType, trueNumberType, 100 ) == 0);
            ASSERT( id == Enum(enumId) );
            int semicol = 0;
            while (semicol < 3 ) {
                ASSERT( m_pos < m_size );
                if (m_buffer[m_pos] == ';') semicol += 1;
                m_pos ++;
            }
#endif

        const unsigned char hasNextBlock = 1u << (CHAR_BIT-1);
        const unsigned char block = hasNextBlock - 1;
        size_t begin = m_pos;
        while ( static_cast<unsigned char>(m_buffer[ m_pos ]) & hasNextBlock  )
        {
            ASSERT( m_pos < m_size );
            m_pos++;
        }
        m_pos++;

        result = 0;
        for (size_t i = m_pos ; i > begin; --i ) {
            unsigned char c = static_cast<unsigned char>( m_buffer[ i-1 ] ); 
            result <<= (CHAR_BIT-1);
            result += (c & block );
        }

        return *this;
    }

    template <class Enum>
    const IPCMesg & read(Enum id, bool & result ) const
    {
        unsigned char interm = '\0';
        (void) read( id, interm );
        result = (interm != '\0');
        return *this;
    }

    template <class Enum>
    const IPCMesg & read(Enum id, void * bytes, size_t size ) const
    {
        (void) id;
#ifndef NDEBUG
        char enumType[101];
        int enumId = -1;
        std::memset(enumType, 0, sizeof(enumType));

        int rc = std::sscanf( m_buffer + m_pos, "%100[^;];%d;;",
                enumType, &enumId );
        ASSERT( rc == 2 );
        ASSERT( std::strcmp( enumType, "blob") == 0 );
        ASSERT( id == Enum(enumId) );
        int semicol = 0;
        while (semicol < 3 ) {
            ASSERT( m_pos < m_size );
            if (m_buffer[m_pos] == ';') semicol += 1;
            m_pos ++;
        }
#endif

        ASSERT( m_pos <= m_size );
        ASSERT( size <= m_size - m_pos );
        std::memcpy( bytes, m_buffer + m_pos, size );
        m_pos += size;
        return *this;
    }


private:
    char * m_buffer;
    size_t m_size;
    mutable size_t m_pos;
};


namespace ipc {

template <class Enum>
IPCMesg<Enum> newMsg( Enum type, char * buf, size_t size)
{
    return IPCMesg<Enum>::writeMsg(type, buf, size);
}

template <class Enum, class Q>
IPCMesg<Enum> recvMsg( Q & queue, char * buf, size_t maxSize)
{
    size_t actualSize = queue.recv( buf, maxSize );
    ASSERT( actualSize <= maxSize );
    return IPCMesg<Enum>::readMsg(buf, actualSize);
}
} // namespace ipc

} } //namespace lpf::mpi

#endif
