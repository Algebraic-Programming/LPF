
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

#ifndef LPF_CORE_MPI_TRYSYNC_HPP
#define LPF_CORE_MPI_TRYSYNC_HPP

#include "communication.hpp"
#include "lpf/core.h"
#include "log.hpp"
#include "linkage.hpp"
#include <stdexcept>
#include <typeinfo>

#define TRYSYNC_BEGIN( comm )\
    {   ::lpf::Communication & _trysync_comm = (comm);  \
        try  

#define TRYSYNC_END( status ) \
    catch( std::bad_alloc & e ) { \
        LOG(1, ": Out of memory exception caught; " << e.what() \
           << "; escalating to LPF_ERR_OUT_OF_MEMORY" ); \
        (status) = std::max<lpf_err_t>( (status), LPF_ERR_OUT_OF_MEMORY); \
    } catch ( std::exception & e ) { \
         LOG(1, ": caught a '" << typeid(e).name() \
                 << "' claiming " << e.what()\
                << "; escalating to LPF_ERR_FATAL" ); \
        (status) = std::max<lpf_err_t>( (status), LPF_ERR_FATAL ); \
    } catch( ... ) { \
         LOG(1, ": caught an exception of unknown type" \
                << "; escalating to LPF_ERR_FATAL" ); \
        (status) = std::max<lpf_err_t>( (status), LPF_ERR_FATAL ); \
    } (status) = _trysync_comm.allreduceMax( status ); }


struct _LPFLIB_LOCAL OtherProcessFailure: public std::runtime_error {
    OtherProcessFailure() : std::runtime_error(
            "An other process experienced an exception" 
            ) {}
};

#define TRYSYNC_RETHROW_END() \
    catch( std::bad_alloc & e ) { \
        LOG(1, ": Out of memory exception caught; " << e.what() );\
        (void) _trysync_comm.allreduceOr(true);\
        throw;\
    } catch ( std::exception & e ) { \
         LOG(1, ": caught a '" << typeid(e).name() \
                 << "' claiming " << e.what());\
        (void) _trysync_comm.allreduceOr(true);\
        throw;\
    } catch( ... ) { \
         LOG(1, ": caught an exception of unknown type" );\
        (void) _trysync_comm.allreduceOr(true);\
        throw;\
    } if ( _trysync_comm.allreduceOr(false) ) \
        throw OtherProcessFailure(); }



#endif
