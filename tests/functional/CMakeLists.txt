
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

# All test sources have file names as bla.c
file(GLOB AllTestSources "*.cpp" )
# All test sources which are specific to some engine are of the form
# bla.pthread.c
file(GLOB AllSpecificTestSources "*.*.cpp")
# All generic test sources don't have the two dots in their name
#file(GLOB AllGenericTestSources "*.c")
file(GLOB AllGenericTestSources "*put_parallel_big.cpp")
list(REMOVE_ITEM AllGenericTestSources ${AllSpecificTestSources})

foreach(LPF_IMPL_ID "ibverbs")
    set(debug ON)
    foreach(debug ON OFF)
        set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

        #file(GLOB ThisEngineSources "*.${LPF_IMPL_ID}.c")

        set(mode)
        if (debug)
            set(mode "_debug")
        endif(debug)

        # add all source files except the ones we don't want
                foreach(testSource ${AllGenericTestSources} ${ThisEngineSources})
                    string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
                    get_filename_component(baseName ${testSource} NAME_WE  )
                    set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")
        #            set(corelib "lpf_core_univ_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")
        #            set(hllib   "lpf_hl${mode}")
        #            set(debuglib   "lpf_debug")

        message("Add gtest : + ${exeName}")
        add_gtest(${exeName} ${LPF_IMPL_ID} ${debug} ${testSource})
            #            add_executable(${exeName} ${testSource})
            #            target_link_libraries(${exeName} ${hllib})
            #            target_link_exe_with_core(${exeName} ${LPF_IMPL_ID})
            #            target_compile_flags(${exeName} PRIVATE ${hllib} "-DLPF_CORE_IMPL_ID=${LPF_IMPL_ID}" )
            #
            #            if (debug)
            #                target_link_libraries(${exeName} ${debuglib})
            #                target_include_directories( ${exeName} BEFORE PRIVATE ../../include/debug )
            #            endif()

            endforeach()
            endforeach(debug)

            #  add_test( NAME "API_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}"
            #      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run.sh" "${LPF_IMPL_ID}"
            #      "${LPF_IMPL_CONFIG}" "${test_output}/api_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}.xml"
            #      )
endforeach(LPF_IMPL_ID)
foreach(LPF_IMPL_ID "ibverbs")
    set(debug ON)
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})
    set(mode)
    if (debug)
        set(mode "_debug")
    endif()

    foreach(testSource ${AllGenericTestSources} ${ThisEngineSources})
        string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

        gtest_discover_tests(${exeName})
    endforeach(testSource)
endforeach(LPF_IMPL_ID)


include_directories(.)

#add_subdirectory(c99)
#add_subdirectory(debug)


option(LPFLIB_MAKE_TEST_DOC "Build the test documentation" OFF)
if (LPFLIB_MAKE_TEST_DOC)
    find_package(Doxygen REQUIRED)

    # Generate Doxygen config file
    configure_file( unittests.cfg.in  unittests.cfg  @ONLY)

    # Run Doxygen
    add_custom_target( testdoc ALL
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html
            COMMENT "Generating API documentation"
            )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/unittests.cfg
            COMMENT "Running Doxygen"
            )
endif()

