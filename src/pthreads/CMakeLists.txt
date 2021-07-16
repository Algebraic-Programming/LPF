
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

if (NOT APPLE)
# The compiler set-up in the MacPorts (2.6.x) collection is a bit strange.
# Here, if POSIX_C_SOURCE is defined, the libstdc++ library will later
# encounter undefined system constants. 
# On Linux systems this line will make some standard POSIX functions available
add_definitions(-D_POSIX_C_SOURCE=200112L)
endif()

set(LPF_IMPL_ID pthread)
set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

# link function that e.g. hybrid implementation can also use to link the raw library.
function(lpf_link_pthread_core target)
    target_link_libraries(${target}
        ${LIB_POSIX_THREADS}
        ${LIB_RT}
    )
    if (HWLOC_FOUND)
        target_link_libraries(${target} ${HWLOC_LIBRARIES})
    endif()
endfunction()


# univ_ stands for universal interface => lpf_exec, lpf_put, etc...
# spec_ stands for specific interface => lpf_pthread_release_exec, lpf_pthread_release_put, etc...
foreach( iface "univ_" "spec_" )

set(libname "lpf_core_${iface}${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")
set(comlib  "lpf_common_${LPFLIB_CONFIG_NAME}")

add_library(raw_${libname} OBJECT
    msgqueue.cpp
    barrier.cpp
    globalstate.cpp
    threadlocaldata.cpp
    ../common/machineparams.cpp
    core.cpp
    init.cpp
)

target_compile_definitions(raw_${libname}
	PRIVATE "LPF_CORE_WARM_UP_PROBE=1"
)

add_library(${libname} SHARED 
        $<TARGET_OBJECTS:raw_${libname}>
        $<TARGET_OBJECTS:${comlib}>
        )
set_target_properties(${libname} PROPERTIES SOVERSION ${SOVERSION}
                                            MACOSX_RPATH TRUE)
 
target_compile_definitions(raw_${libname} PRIVATE 
        "LPF_CORE_IMPL_ID=${LPF_IMPL_ID}"
        "LPF_CORE_IMPL_CONFIG=${LPF_IMPL_CONFIG}"
)
if (iface STREQUAL "spec_")
    target_compile_definitions(raw_${libname}
       PRIVATE "LPF_CORE_STATIC_DISPATCH=1"
               "LPF_CORE_STATIC_DISPATCH_ID=${LPF_IMPL_ID}"
               "LPF_CORE_STATIC_DISPATCH_CONFIG=${LPF_IMPL_CONFIG}"
    )
    target_compile_definitions(${libname}
       INTERFACE "LPF_CORE_STATIC_DISPATCH=1"
                 "LPF_CORE_STATIC_DISPATCH_ID=${LPF_IMPL_ID}"
                 "LPF_CORE_STATIC_DISPATCH_CONFIG=${LPF_IMPL_CONFIG}"
    )
endif()

target_include_directories(raw_${libname} INTERFACE
        $<INSTALL_INTERFACE:${INSTALL_HEADERS}>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

target_link_libraries(${libname}
    ${LIB_POSIX_THREADS}
   )

if (HWLOC_FOUND)
    target_compile_definitions(raw_${libname} PRIVATE "LPF_CORE_PTHREAD_USE_HWLOC=1")
    target_include_directories(raw_${libname} PRIVATE ${HWLOC_INCLUDE_DIR})
endif()

lpf_link_pthread_core(${libname})

install(TARGETS ${libname} EXPORT lpf
        RUNTIME DESTINATION ${INSTALL_BIN}
        LIBRARY DESTINATION ${INSTALL_LIB}
        ARCHIVE DESTINATION ${INSTALL_LIB}
       )

endforeach()
