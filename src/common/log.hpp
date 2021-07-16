
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

#ifndef LPFLIB_LOG_HPP
#define LPFLIB_LOG_HPP

#include <iostream>
#include <sstream>
#include "assert.hpp"
#include "linkage.hpp"

namespace lpf {

    extern _LPFLIB_LOCAL 
    std::string log_timestamp();

    extern _LPFLIB_LOCAL
    int log_level();

    extern _LPFLIB_LOCAL
    void log_error() ;

}

#if !defined(LPF_CORE_LOG_LEVEL) || LPF_CORE_LOG_LEVEL == 0
  #define LOG(level, message ) \
    /*lint -e{717} we use the do{} while(0) idom here*/\
    do { /*lint -e{866} we use sizeof here to use variable 'x' */\
        (void) sizeof(::lpf::log_stream(level) << message); } while(0) 
#else

  #if defined LPF_CORE_IMPL_ID && defined LPF_CORE_IMPL_CONFIG
    #define LPF_INTERNAL_IDENTIFY2( a, b )   "< " #a "_" #b " >"
    #define LPF_INTERNAL_IDENTIFY( a, b )   LPF_INTERNAL_IDENTIFY2(a, b)
  #else
    #define LPF_INTERNAL_IDENTIFY( a, b )  ""
  #endif

  /*lint -estring( 665, LOG ) allow unparenthesized use of message */
  /*lint -emacro(717, LOG) we use the do{} while(0) idom here*/ 
  /*lint -emacro(1766, LOG) we use catch(...) so that LOG is safe in destructors */
  /*lint -emacro(845, LOG) Do not warn about switched off log messages */
  /*lint -emacro(2650, LOG) Do now warn about switched off log messages */
  #define LOG( level, message )                            \
     do { LPFLIB_IGNORE_TAUTOLOGIES                  \
          try {                                            \
          if ( (level) <= LPF_CORE_LOG_LEVEL && (level) <= ::lpf::log_level() ) {\
              ::std::ostringstream __s;                    \
              __s << ::lpf::log_timestamp()  << __FILE__ << ":" << __LINE__ << ": "     \
                  << LPF_INTERNAL_IDENTIFY( LPF_CORE_IMPL_ID, LPF_CORE_IMPL_CONFIG) \
                  << message;                              \
              __s << '\n';                                 \
              ::std::cerr << __s.str() ;                   \
          }} catch(...) { try { ::lpf::log_error(); } catch(...){}} \
          LPFLIB_RESTORE_WARNINGS                    \
     } while(0) 
#endif

#endif
