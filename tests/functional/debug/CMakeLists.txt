
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
file(GLOB AllTestSources "*.c" )
# All test sources which are specific to some engine are of the form
# bla.pthread.c
file(GLOB AllSpecificTestSources "*.*.c")
# All generic test sources don't have the two dots in their name
file(GLOB AllGenericTestSources "*.c")
list(REMOVE_ITEM AllGenericTestSources ${AllSpecificTestSources})

foreach(LPF_IMPL_ID ${ENGINES})
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

    file(GLOB ThisEngineSources "*.${LPF_IMPL_ID}.c")

    # add all source files except the ones we don't want
    foreach(testSource ${AllGenericTestSources} ${ThisEngineSources})
        string(REGEX REPLACE ".c$" "" baseName ${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")
        set(corelib "lpf_core_univ_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")
        set(debuglib   "lpf_debug")

        add_executable(${exeName} ${testSource})
        target_include_directories( ${exeName} BEFORE PRIVATE ../../../include/debug )
        target_link_libraries(${exeName} ${debuglib})
        target_link_exe_with_core(${exeName} ${LPF_IMPL_ID})
        target_compile_flags(${exeName} PRIVATE "-DLPF_CORE_IMPL_ID=${LPF_IMPL_ID}" )
    endforeach()

endforeach(LPF_IMPL_ID)

