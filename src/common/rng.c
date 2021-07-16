
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

#include "rng.h"
#include <math.h>
#include "assert.hpp"

static uint64_t int_pow( uint64_t x, uint64_t n )
{
    uint64_t y = 1;
    uint64_t power = x;
    uint64_t m = n;
    uint64_t i;

    for (i = 0; i < n; ++i)
    {
        if ( m % 2 == 1)
            y = y * power;

        m = m / 2;

        // square the power
        power = power * power;
    }

    return y;
}

rng_state_t rng_create( uint64_t seed )
{
  rng_state_t state;
  // According to Wikipedia the choice of these parameters
  // come from MMIX by Donald Knuth... I should test
  // how random this is..
  state.multiplier = 6364136223846793005ull;
  state.increment = 1442695040888963407ull;
  state.value = seed;
  return state;
}

uint64_t rng_next( rng_state_t * state  )
{
    state->value = state->multiplier * state->value + state->increment ;
    return state->value;
}

static void mult64( uint64_t a, uint64_t b, uint64_t * x_high, uint64_t * x_low)
{
    uint64_t a_high = a >> 32;
    uint64_t b_high = b >> 32;
    uint64_t a_low = a & UINT32_MAX;
    uint64_t b_low = b & UINT32_MAX;
    uint64_t x_middle = a_low * b_high + a_high * b_low;
    *x_high = a_high * b_high + (x_middle >> 32) ;
    *x_low = a_low * b_low + (x_middle & UINT32_MAX);
}

static uint64_t div64( uint64_t x_high, uint64_t x_low, uint64_t nom)
{
    if (x_high == 0) return x_low / nom;

    double p = pow(2.0, 64);
    uint64_t estimate = (uint64_t) ( (x_high * p + x_low)/(double) nom);

    uint64_t y_high, y_low;
    while( (void)mult64( nom, estimate, &y_high, &y_low), y_high >= x_high || (y_high == x_high && y_low > x_low))
        estimate -= 1;

    ASSERT( y_high <= x_high || (y_high == x_high && y_low <= x_low));

    while( (void)mult64( nom, estimate, &y_high, &y_low), y_high < x_high || (y_high == x_high && y_low <= x_low))
        estimate += 1;

    return estimate - 1;
}

void rng_parallelize( rng_state_t * state, int pid_in, int nprocs_in )
{
    const uint64_t pid = (uint64_t)pid_in;
    const uint64_t nprocs = (uint64_t)nprocs_in;
    uint64_t a = state->multiplier;
    uint64_t b = a - 1;
    uint64_t c = state->increment;
    uint64_t a_nprocs = int_pow( a, nprocs);
    uint64_t a_pid = int_pow( a, pid );

    uint64_t x_high, x_low;
    mult64( a_pid-1, c, &x_high, &x_low);
    state->value = a_pid * state->value + div64(x_high, x_low, b);
    state->multiplier = a_nprocs;

    mult64( a_nprocs-1, c, &x_high, &x_low);
    state->increment = div64( x_high, x_low, b);
}


