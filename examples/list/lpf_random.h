
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

#ifndef LPF_RAND_H
#define LPF_RAND_H

#include <lpf/core.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {

#endif

typedef struct lpf_rand_state {
    // compute modulo 2^31-1
  uint64_t multiplier;
  uint64_t increment;
  uint64_t value;
} lpf_rand_state_t;

/** Initialize a parallel pseudo random number generator, using the linear
 * congruential method. 
 *
 * \param[out] state The state of the random number generator
 * \param[in]  seed  The seed of the generator, which must be the same on
 *                   all processes
 * \param[in]  pid   The pid of this process
 * \param[in]  nprocs The number of processes
 */
void lpf_rand_create( lpf_rand_state_t * state,
        uint64_t seed, lpf_pid_t pid, lpf_pid_t nprocs  );


/** Generates the next pseudo random number in the range [0, UINT64_MAX] 
 *
 * \param[in,out] state The state of the random number generator
 * 
 * \returns The next pseudo random number between 0 and UINT64_MAX (inclusive)
 *
 **/
uint64_t lpf_rand_next( lpf_rand_state_t * state  );


#ifdef __cplusplus
}  // extern "C"
#endif


#endif
