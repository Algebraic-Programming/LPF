
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

foreach(LPF_IMPL_ID ${ENGINES})
foreach(debug ON OFF)
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

    set(mode)
    if (debug) 
       set(mode "_debug")
    endif()

    file(GLOB TestSources "*.c")
    foreach(testSource ${TestSources})
        string(REGEX REPLACE ".c$" "" exeName ${testSource})
        get_filename_component(exeName ${testSource} NAME_WE  )
        set(exeName "${exeName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

        set(corelib "lpf_core_univ_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")
        set(hllib   "lpf_hl${mode}")
        set(debuglib   "lpf_debug")
        add_executable(${exeName} ${testSource})
        target_link_exe_with_core(${exeName} ${LPF_IMPL_ID})
        target_link_libraries(${exeName} ${hllib})
	set_target_properties(${exeName} PROPERTIES
		C_STANDARD 99
		C_STANDARD_REQUIRED YES
	)
        target_compile_flags(${exeName} PRIVATE ${hllib})
        if (debug)
            target_link_libraries(${exeName} ${debuglib})
            target_include_directories( ${exeName} BEFORE PRIVATE ../../../include/debug )
        endif()
    endforeach()
endforeach(debug)
endforeach(LPF_IMPL_ID)

