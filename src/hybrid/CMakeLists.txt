
#
#   Copyright 2021 Huawei Technologies Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if (HYBRID_ENGINE_ENABLED)

set(LPF_IMPL_ID hybrid)
set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

set(LPFLIB_HYBRID_MPI_ENGINE "ibverbs" CACHE STRING
        "Choice of MPI engine to use for inter-process communication")
set(mpi_engine  "${LPFLIB_HYBRID_MPI_ENGINE}" )
message( STATUS "Hybrid implementation's multi-node layer is '${mpi_engine}'")

set(libname      "lpf_core_univ_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")
set(pthread_core "lpf_core_spec_pthread_${LPF_IMPL_CONFIG}" )
set(mpi_core     "lpf_core_spec_${mpi_engine}_${LPF_IMPL_CONFIG}" )
set(comlib       "lpf_common_${LPFLIB_CONFIG_NAME}")

#Always build the shared library, because we need that for lpfrun
add_library(${libname} SHARED
        lpfexec.cpp
        dispatch.cpp
        ../common/machineparams.cpp
        core.cpp
        $<TARGET_OBJECTS:raw_${pthread_core}>
        $<TARGET_OBJECTS:raw_${mpi_core}>
        $<TARGET_OBJECTS:${comlib}>
)
set_target_properties(${libname} PROPERTIES SOVERSION ${SOVERSION})

target_compile_definitions(${libname} 
        PRIVATE "LPF_CORE_MULTI_NODE_ENGINE=${mpi_engine}"
                "LPF_CORE_WARM_UP_PROBE=1"
                "LPF_CORE_IMPL_ID=${LPF_IMPL_ID}"
                "LPF_CORE_IMPL_CONFIG=${LPF_IMPL_CONFIG}"
)

target_compile_flags(${libname} PUBLIC
        ${pthread_core}
        ${mpi_core}
        )

lpf_link_pthread_core(${libname})
lpf_link_mpi_core(${libname} ${mpi_engine})

install(TARGETS ${libname} EXPORT lpf
        RUNTIME DESTINATION ${INSTALL_BIN}
        LIBRARY DESTINATION ${INSTALL_LIB}
        ARCHIVE DESTINATION ${INSTALL_LIB}
       )

endif()
