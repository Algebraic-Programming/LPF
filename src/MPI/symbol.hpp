
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

#ifndef LPF_CORE_MPI_SYMBOL_HPP
#define LPF_CORE_MPI_SYMBOL_HPP

#include <string>
#include <stdexcept>

#if __cplusplus >= 201103L
#include <memory>
#else
#include <tr1/memory>
#endif

#include "linkage.hpp"

namespace lpf {

class _LPFLIB_LOCAL Symbol
{
public:
    struct LookupException : public std::runtime_error 
    { LookupException( const std::string & ); };

    Symbol();
    explicit Symbol( const void * symbol );
    explicit Symbol( const std::string & file, const std::string & name);
    
    void * getSymbol() const ; 
    std::string getSymbolName() const;
    std::string getFileName() const;

    template <class T>
    void getSymbol( T * & x ) const
    {
        void * const ptr = getSymbol();
        x = *reinterpret_cast<T * const *>(&ptr); 
    }

private:
    struct Impl {
        ~Impl();
        void * m_fileHandle;
        void * m_symbol;
        std::string m_fileName;
        std::string m_symbolName;
    };
#if __cplusplus >= 201103L
    std::shared_ptr< Impl > m_impl;
#else
    std::tr1::shared_ptr< Impl > m_impl;
#endif
};

}

#endif

