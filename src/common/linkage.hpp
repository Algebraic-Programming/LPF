
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

#ifndef LPFLIB_LINKAGE_HPP
#define LPFLIB_LINKAGE_HPP

// Symbol visibility macros
// copied from https://gcc.gnu.org/wiki/Visibility
//
#if !defined _LPFLIB_LOCAL || ! defined _LPFLIB_API

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
  #define _LPFLIB_HELPER_DLL_IMPORT __declspec(dllimport)
  #define _LPFLIB_HELPER_DLL_EXPORT __declspec(dllexport)
  #define _LPFLIB_HELPER_DLL_LOCAL

  #define _LPFLIB_HELPER_VAR_IMPORT __declspec(dllimport)
  #define _LPFLIB_HELPER_VAR_EXPORT __declspec(dllexport)
#else
  #if __GNUC__ >= 4
    #define _LPFLIB_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define _LPFLIB_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define _LPFLIB_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define _LPFLIB_HELPER_DLL_IMPORT
    #define _LPFLIB_HELPER_DLL_EXPORT
    #define _LPFLIB_HELPER_DLL_LOCAL
  #endif
  #define _LPFLIB_HELPER_VAR_IMPORT
  #define _LPFLIB_HELPER_VAR_EXPORT
#endif

// Now we use the generic helper definitions above to define _LPFLIB_API and _LPFLIB_LOCAL.
// _LPFLIB_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for static build)
// _LPFLIB_LOCAL is used for non-api symbols.

#ifdef _LPFLIB_DLL // defined if _LPFLIB is compiled as a DLL
  #ifdef _LPFLIB_DLL_EXPORTS // defined if we are building the _LPFLIB DLL (instead of using it)
    #define _LPFLIB_API _LPFLIB_HELPER_DLL_EXPORT
    #define _LPFLIB_VAR _LPFLIB_HELPER_VAR_EXPORT
  #else
    #define _LPFLIB_API _LPFLIB_HELPER_DLL_IMPORT
    #define _LPFLIB_VAR _LPFLIB_HELPER_VAR_IMPORT
  #endif // _LPFLIB_DLL_EXPORTS
  #define _LPFLIB_LOCAL _LPFLIB_HELPER_DLL_LOCAL
#else // _LPFLIB_DLL is not defined: this means _LPFLIB is a static lib.
  #define _LPFLIB_API
  #define _LPFLIB_VAR
  #define _LPFLIB_LOCAL
#endif // _LPFLIB_DLL

#endif // if not LPFLIB_LOCAL or API were not defined

#if ! defined _LPFLIB_SPMD
  #if __GNUC__ >= 4
    #define _LPFLIB_SPMD __attribute__((visibility("default")))
  #else
    #define _LPFLIB_SPMD
  #endif
#endif

#endif
