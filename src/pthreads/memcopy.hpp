
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

#ifndef PLATFORM_CORE_MEMCOPY_HPP
#define PLATFORM_CORE_MEMCOPY_HPP

#include <cstring>

#include "types.hpp"
#include "memreg.hpp"
#include "assert.hpp"
#include "linkage.hpp"

namespace lpf
{

class _LPFLIB_LOCAL MemCopy
{
public:
    MemCopy();

    MemCopy( pid_t srcPid, 
             memslot_t srcSlot, size_t srcOffset, 
             memslot_t dstSlot, size_t dstOffset, 
            size_t size );

    template <class Reg>
    void execute( pid_t dstPid, const Reg & reg ) const // nothrow
    {
        ASSERT(m_srcOffset + m_size <= reg[m_srcPid].lookup(m_srcSlot).size);
        ASSERT(m_dstOffset + m_size <= reg[dstPid].lookup(m_dstSlot).size);
        char * src = reg[m_srcPid].lookup(m_srcSlot).addr + m_srcOffset ;
        char * dst = reg[dstPid].lookup( m_dstSlot).addr + m_dstOffset ;
        ASSERT( src + m_size <= dst || dst + m_size <= src);
        std::memcpy( dst, src, m_size );
    }

    template <class Reg>
    class Processor
    {
    public:
        explicit Processor( const Reg & reg) // nothrow
            : m_reg( reg ) {}

        void operator()( pid_t myPid, const MemCopy & copy ) const // nothrow
        { copy.execute( myPid, m_reg ); }

    private:
        const Reg & m_reg;
    };

private:
    pid_t m_srcPid;
    memslot_t m_srcSlot, m_dstSlot;
    size_t m_srcOffset, m_dstOffset;
    size_t m_size;
};


} // namespace lpf

#endif
