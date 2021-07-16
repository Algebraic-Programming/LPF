
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

file(GLOB HLSources "*.c")
file(GLOB HLSourcesCPP "*.cpp")
set(comlib  "lpf_common_${LPFLIB_CONFIG_NAME}")
set(debuglib "lpf_debug")

add_library(lpf_hl
    ${HLSources}
    ${HLSourcesCPP}
    $<TARGET_OBJECTS:${comlib}>
)

add_library(lpf_hl_debug
    ${HLSources}
    ${HLSourcesCPP}
    $<TARGET_OBJECTS:${comlib}>
)
target_include_directories(lpf_hl_debug BEFORE PRIVATE ../../include/debug )
target_link_libraries(lpf_hl_debug ${debuglib} )

set_target_properties(lpf_hl lpf_hl_debug
        PROPERTIES SOVERSION ${SOVERSION}
	C_STANDARD 99
	C_STANDARD_REQUIRED YES
        )
install(TARGETS lpf_hl lpf_hl_debug EXPORT lpf
    RUNTIME DESTINATION ${INSTALL_BIN}
    LIBRARY DESTINATION ${INSTALL_LIB}
    ARCHIVE DESTINATION ${INSTALL_LIB}
)


