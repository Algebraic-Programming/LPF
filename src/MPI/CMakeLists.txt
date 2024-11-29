
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

if (MPI_FOUND)

    set(MPI_ENGINES mpimsg)
    if (MPI_RMA)
        list(APPEND MPI_ENGINES mpirma)
    endif()

    if (ENABLE_IBVERBS)
        list(APPEND MPI_ENGINES ibverbs)
    endif()

    if (MPI_IBARRIER)
      add_definitions("-DMPI_HAS_IBARRIER=1")
    endif()

    add_executable(        lpf_proxy_dummy proxydummy.c)
    target_link_libraries( lpf_proxy_dummy ${MPI_C_LIBRARIES})
    set_target_properties( lpf_proxy_dummy PROPERTIES LINK_FLAGS
            "${MPI_C_LINK_FLAGS}" )
    target_include_directories( lpf_proxy_dummy PRIVATE ${MPI_C_INCLUDE_PATH})
    install( TARGETS lpf_proxy_dummy RUNTIME DESTINATION ${INSTALL_HELPERS} )

    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})


    # univ_ stands for universal interface => lpf_exec, lpf_put, etc...
    # spec_ stands for specific interface => lpf_mpimsg_release_exec, lpf_mpimsg_release_put, etc...
    foreach (iface  "univ_" "spec_" )
        foreach (LPF_IMPL_ID ${MPI_ENGINES})
            set(libname "lpf_core_${iface}${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")
            set(comlib  "lpf_common_${LPFLIB_CONFIG_NAME}")

            set(ibverbs_sources)
            if (LPF_IMPL_ID STREQUAL ibverbs)
            set(ibverbs_sources ibverbs.cpp)
        endif()

        add_library(raw_${libname} OBJECT
                memorytable.cpp
                mesgqueue.cpp
                mpilib.cpp
                symbol.cpp
                process.cpp
                interface.cpp
                ../common/machineparams.cpp
                core.cpp
                dynamichook.cpp
                spall2all.c
                messagesort.cpp
                spall2all.cpp
                init.cpp
                ${ibverbs_sources}
            )


        target_compile_flags(raw_${libname} 
                INTERFACE "-fPIC")

        target_compile_definitions(raw_${libname} 
                PRIVATE "LPF_CORE_MPI_USES_${LPF_IMPL_ID}=1"
                        "LPF_CORE_WARM_UP_PROBE=1"
                        "LPF_CORE_IMPL_ID=${LPF_IMPL_ID}"
                        "LPF_CORE_IMPL_CONFIG=${LPF_IMPL_CONFIG}"
        )
        target_include_directories(raw_${libname} 
             PRIVATE  ${MPI_C_INCLUDE_PATH}
        )
        if (iface STREQUAL "spec_")
            target_compile_definitions(raw_${libname} 
                   PRIVATE "LPF_CORE_STATIC_DISPATCH=1"
                           "LPF_CORE_STATIC_DISPATCH_ID=${LPF_IMPL_ID}"
                           "LPF_CORE_STATIC_DISPATCH_CONFIG=${LPF_IMPL_CONFIG}"
            )
        endif()

        #Always build the shared library, because we need that for the lpfrun
        add_library(${libname} SHARED
                $<TARGET_OBJECTS:raw_${libname}> 
                $<TARGET_OBJECTS:${comlib}>
        )
        set_target_properties(${libname} PROPERTIES SOVERSION ${SOVERSION}
                                                    MACOSX_RPATH TRUE)

        target_compile_flags(${libname} 
                INTERFACE "-fPIC")

        if (iface STREQUAL "spec_")
            target_compile_definitions(${libname} 
               INTERFACE "LPF_CORE_STATIC_DISPATCH=1"
                         "LPF_CORE_STATIC_DISPATCH_ID=${LPF_IMPL_ID}"
                         "LPF_CORE_STATIC_DISPATCH_CONFIG=${LPF_IMPL_CONFIG}"
            )
        endif()
        target_include_directories(${libname} 
             PUBLIC   ${MPI_C_INCLUDE_PATH}
             INTERFACE $<INSTALL_INTERFACE:${INSTALL_HEADERS}>
                       $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        )

    endforeach(LPF_IMPL_ID)
    endforeach(iface)

    # link function that e.g. hybrid implementation can also use.
    function(lpf_link_mpi_core target engine)
        target_link_libraries(${target} 
                ${MPI_C_LIBRARIES}
                ${LIB_MATH}
                ${LIB_DL}
                ${LIB_RT}
                ${LIB_POSIX_THREADS}
        )

        if (engine STREQUAL ibverbs)
           target_link_libraries(${target} ${LIB_IBVERBS})
        endif()
    endfunction()


    foreach (LPF_IMPL_ID ${MPI_ENGINES})
        set(libname "lpf_core_univ_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")

        lpf_link_mpi_core(${libname} ${LPF_IMPL_ID})

        install(TARGETS ${libname} EXPORT lpf
                RUNTIME DESTINATION ${INSTALL_BIN}
                LIBRARY DESTINATION ${INSTALL_LIB}
                ARCHIVE DESTINATION ${INSTALL_LIB}
               )
    endforeach()
  
    include_directories(${MPI_C_INCLUDE_PATH})
    # add a test for dynamichook 
    if (NOT IS_OPENMPI AND LPF_ENABLE_TESTS)
        add_gtest(dynamichook.t "mpimsg" ON
            ${CMAKE_CURRENT_SOURCE_DIR}/dynamichook.t.cpp 
            ${CMAKE_CURRENT_SOURCE_DIR}/dynamichook.cpp 
            ${CMAKE_CURRENT_SOURCE_DIR}/mpilib.cpp)
        
            configure_file( dynamichook.t.sh.in dynamichook.t.sh @ONLY)
            set( dynamic_hook_t_sh "${CMAKE_CURRENT_BINARY_DIR}/dynamichook.t.sh")
            add_test(NAME dynamichook_1proc
                COMMAND bash ${dynamic_hook_t_sh} 1)
            # We set all dynamichook tests to run in serial mode, without any other tests,
            # since these tests occupy the same port and would block each other
            set_tests_properties( dynamichook_1proc PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
            add_test(NAME dynamichook_2proc
                COMMAND bash ${dynamic_hook_t_sh} 2)
            set_tests_properties( dynamichook_2proc PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
            add_test(NAME dynamichook_3proc
                COMMAND bash ${dynamic_hook_t_sh} 3)
            set_tests_properties( dynamichook_3proc PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
            add_test(NAME dynamichook_10proc
                COMMAND bash ${dynamic_hook_t_sh} 10)
            set_tests_properties( dynamichook_10proc PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    endif()

# Other unit tests
    if (ENABLE_IBVERBS AND LPF_ENABLE_TESTS)
        add_gtest( ibverbs_test "ibverbs" ON ${CMAKE_CURRENT_SOURCE_DIR}/ibverbs.t.cpp 
            ${CMAKE_CURRENT_SOURCE_DIR}/ibverbs.cpp 
            ${CMAKE_CURRENT_SOURCE_DIR}/mpilib.cpp)
    endif()

    foreach (engine ${MPI_ENGINES})
        add_gtest( spall2all_test_${engine} ${engine} ON
            ${CMAKE_CURRENT_SOURCE_DIR}/spall2all.t.cpp 
            ${CMAKE_CURRENT_SOURCE_DIR}/spall2all.c 
            ${CMAKE_CURRENT_SOURCE_DIR}/spall2all.cpp 
            ${CMAKE_CURRENT_SOURCE_DIR}/mpilib.cpp)

        add_gtest( dall2all_test_${engine} ${engine} ON
            ${CMAKE_CURRENT_SOURCE_DIR}/dall2all.t.cpp 
            ${CMAKE_CURRENT_SOURCE_DIR}/mpilib.cpp)

        if (MPI_IBARRIER)
            add_gtest( hall2all_test_${engine} ${engine} ON
                ${CMAKE_CURRENT_SOURCE_DIR}/hall2all.t.cpp 
                ${CMAKE_CURRENT_SOURCE_DIR}/mpilib.cpp)
        endif()

        add_gtest( messagesort_test_${engine} ${engine} ON
            ${CMAKE_CURRENT_SOURCE_DIR}/messagesort.t.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/messagesort.cpp)

        add_gtest( ipcmesg_test_${engine} ${engine} ON
            ${CMAKE_CURRENT_SOURCE_DIR}/ipcmesg.t.cpp)
    endforeach()

endif(MPI_FOUND)

