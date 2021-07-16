
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

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <lpf/core.h>

#include "assert.hpp"

#define MAX_FILENAME_LENGTH 1024

int intMax( int a, int b ) { return a < b? b : a; }
size_t sizeMax( size_t a, size_t b ) { return a < b? b : a; }
size_t sizeMin( size_t a, size_t b ) { return a < b? a : b; }

int cmpDouble( const void * a, const void * b)
{
    double x= * (const double *) a;
    double y = *(const double *) b;
    if (x < y) return -1;
    else if (x > y ) return 1;
    else return 0;
}

int timespec_cmp( struct timespec t0, struct timespec t1)
{
   if (t0.tv_sec != t1.tv_sec)
       return t0.tv_sec - t1.tv_sec;
   else
       return t0.tv_nsec - t1.tv_nsec;
}

double timespec_to_seconds( struct timespec t)
{
    return t.tv_sec + 1e-9 * t.tv_nsec;
}

struct timespec timespec_from_seconds( double seconds)
{
    struct timespec t;
    t.tv_sec = seconds;
    t.tv_nsec = (seconds - t.tv_sec)*1e9;
    return t;
}

struct timespec timespec_add( struct timespec t0, struct timespec t1 )
{
   const long billion = 1000000000;

   t0.tv_sec += t1.tv_sec ;
   if (t0.tv_nsec > billion - t1.tv_nsec ) {
       t0.tv_sec += 1;
       t0.tv_nsec = t0.tv_nsec + t1.tv_nsec - billion;
   }
   else {
       t0.tv_nsec = t0.tv_nsec + t1.tv_nsec;
   }
   return t0;
}


struct timespec timespec_diff( struct timespec t0, struct timespec t1)
{
    t0.tv_sec -= t1.tv_sec;
    if (t0.tv_nsec >= t1.tv_nsec) {
        t0.tv_nsec -= t1.tv_nsec;
    }
    else {
        t0.tv_nsec += 1000000000 - t1.tv_nsec ;
        t0.tv_sec -= 1;
    }
    return t0;
}


struct timespec timespec_div( struct timespec t, int n)
{
   const long billion = 1000000000;
   ldiv_t d = ldiv( t.tv_sec, n );
   t.tv_sec = d.quot;
   t.tv_nsec = ((uint64_t) d.rem * billion  + t.tv_nsec) / n;
   return t;
}

struct timespec timespec_double( struct timespec t )
{
    const long billion = 1000000000;
    int mult = 2;
    t.tv_sec *= mult;
    if ( t.tv_nsec > billion  / mult ) {
        ldiv_t d = ldiv( t.tv_nsec * mult, billion);
        t.tv_sec += d.quot;
        t.tv_nsec = d.rem;
    }
    else {
        t.tv_nsec *= mult;
    }
    return t;
}


typedef struct {
    size_t S, N, M, K, wordSize;
    double CI;
    int verbose;
    size_t i;
    struct timespec period;
    double  g, L, T0, r;
    double eps_g, eps_L, eps_T0, eps_r; // significant digits
    lpf_pid_t nprocs;
    int checkpointLoad;
    struct timespec checkpointInterval;
    char checkpointName[MAX_FILENAME_LENGTH];
    char message[10000];
} params_t;

typedef struct {
    size_t S, N, M, K, wordSize;
    struct timespec period;
    double CI;
    int verbose, view;
    lpf_pid_t nprocs;
    struct timespec checkpointInterval;
    char checkpointName[MAX_FILENAME_LENGTH];
} cmdline_params_t;


int readBlock( FILE * file, void * ptr, size_t size)
{
    int e = 0;
    if (ptr)
    {
        void * mem = malloc( size );
        if (mem) {
            size_t S = fread( mem, size, 1, file);
            e = (S != 1);
        }
        else
        {
            e = 1;
        }
        if (e == 0)
            memcpy( ptr, mem, size);

        free(mem);
    }
    else
    {
        e = fseek( file, size, SEEK_CUR );
    }
    return e;
}


int writeBlock( FILE * file, const void * ptr, size_t size)
{
    int e = 0;
    if (ptr)
    {
        size_t S = fwrite( ptr, size, 1, file);
        e = (S != 1);
    }
    else
    {
        e = fseek( file, size, SEEK_CUR );
    }
    return e;
}

int readCheckpoint( const char * filename,
        params_t * params, double *Ts,
       struct timespec * T_tot, size_t * T_nr,
       struct timespec * T_gtot, size_t * T_gnr)
{
    FILE * file = fopen(filename, "r");
    if (!file)
        return 1;

    int e = 0;
    e = e || readBlock( file, params, sizeof(params_t));

    if (params)
    {
        const size_t K = params->K;
        const size_t M = params->M;
        e = e || readBlock( file, Ts,    5*M*K*sizeof(Ts[0]) );
        e = e || readBlock( file, T_nr,  5*M*sizeof(T_nr[0]) );
        e = e || readBlock( file, T_tot, 5*M*sizeof(T_tot[0]) );
        e = e || readBlock( file, T_gnr,  5*sizeof(T_gnr[0]) );
        e = e || readBlock( file, T_gtot, 5*sizeof(T_gtot[0]) );
    }

    fclose(file);
    return e;
}

int writeCheckpoint( const char * filename,
       const params_t * params, const double *Ts,
       const struct timespec * T_tot, const size_t * T_nr,
       const struct timespec * T_gtot, const size_t * T_gnr)
{
    int e = 0;
    // backup the previous checkpoint
    if (strlen(filename) < MAX_FILENAME_LENGTH - 10 ) {
        char backup[MAX_FILENAME_LENGTH];
        snprintf(backup, MAX_FILENAME_LENGTH, "%s.bak",
               filename );
        e = rename( filename, backup );

        // of course, it is not a failure when there was no previous checkpoint
        if (e && errno == ENOENT)
            e = 0;
    }

    // write the checkpoint
    FILE * file = fopen(filename, "w");
    if (!file)
        return 1;

    e = e || writeBlock( file, params, sizeof(params_t));

    if (params)
    {
        const size_t K = params->K;
        const size_t M = params->M;
        e = e || writeBlock( file, Ts,    5*M*K*sizeof(Ts[0]) );
        e = e || writeBlock( file, T_nr,  5*M*sizeof(T_nr[0]) );
        e = e || writeBlock( file, T_tot, 5*M*sizeof(T_tot[0]) );
        e = e || writeBlock( file, T_gnr,  5*sizeof(T_gnr[0]) );
        e = e || writeBlock( file, T_gtot, 5*sizeof(T_gtot[0]) );
    }

    fclose(file);
    return e;
}

struct timespec measure_memcpy( char * input, char *output,
        struct timespec duration, size_t H, size_t wordSize,
        size_t *n )
{

   *n = 0;
   struct timespec s0, s1;
   clock_gettime( CLOCK_MONOTONIC, &s0);
   while (1) {
       for (size_t j = 0; j < H; ++j)
       {
           memcpy( output + wordSize * j, input + wordSize * j, wordSize) ;
       }

       clock_gettime( CLOCK_MONOTONIC, &s1);
       *n += 1;

       if ( timespec_cmp( timespec_diff( s1, s0), duration) > 0)
           break;
   }

   return timespec_diff( s1, s0) ;
}


struct timespec measure( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs,
        lpf_memslot_t inputSlot, lpf_memslot_t outputSlot,
        int * stop, lpf_memslot_t stopSlot, struct timespec duration,
        size_t  H, size_t  wordSize, size_t * n )
{
   lpf_err_t rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
   ASSERT( LPF_SUCCESS == rc );

   *n = 0;
   struct timespec s0, s1;
   *stop = 0;
   lpf_sync( lpf, LPF_SYNC_DEFAULT );
   clock_gettime( CLOCK_MONOTONIC, &s0);
   while ( ! *stop || *n < 2 ) {
       clock_gettime( CLOCK_MONOTONIC, &s1 );
       *n += 1;
       for (size_t j = 0; j < H; ++j)
       {
           lpf_pid_t dstPid = (pid + j + 1) % nprocs;
           size_t dstOff = pid+j/nprocs*nprocs;
           lpf_put( lpf, inputSlot, j*wordSize,
                   dstPid, outputSlot, dstOff*wordSize ,
                  wordSize, LPF_MSG_DEFAULT );
       }
       if ( timespec_cmp( timespec_diff( s1, s0), duration) > 0) {
           *stop = 1;
           for (lpf_pid_t i = 0; i < nprocs; ++i)
               if (i != pid)
                 lpf_put( lpf, stopSlot, 0, i, stopSlot, 0, sizeof(*stop),
                         LPF_MSG_DEFAULT);
       }
       lpf_sync( lpf, LPF_SYNC_DEFAULT );
   }
   *n -= 1;

   return timespec_diff( s1, s0) ;
}

// The CDF for a standard normal distributed random variable
double phi( double t)
{
  return 0.5 * erfc( - t * M_SQRT1_2 );
}

// The PDF for a standard normal distributed random variable
double dphi( double t)
{
  return exp(-0.5 * t * t) / sqrt( 2 * M_PI);
}

// find the radius of the confidence interval for a given probability
// of a standard normal distributed random variable
double confIntervalNormal( double prob, double error )
{
    ASSERT( prob > 0.0);
    ASSERT( prob < 1.0 );
    // using newton-raphson to find 'x' such that
    // P( |X| < x ) =  prob
    // for X is standard normal distributed random variable
    double y = prob;
    double x = 0.0;
    double f = 0.0;
    double df = 0.0;
    do {
        f = phi(x)-phi(-x);
        df = 2.0*dphi(x);
        x += (y - f) / df;
    } while ( f - y > error );

    return x;
}

double min( double x, double y)
{ return x < y ? x : y; }


// estimate of how far off the normal distribution is for a skewed distribution
// Shevtsova 2011
double shevtsova( double beta3, double n)
{
    return min( 0.3328 * (beta3 + 0.429) / sqrt(n),
                0.33554 * (beta3 + 0.415) / sqrt(n) );
}




//int takeStats( size_t N, size_t distrSize, double *mean, double *stddev, double * distr)
void takeStats( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args )
{
   // pid 0 loads arguments
   params_t params;
   double *Ts = NULL;
   double *T_avg = NULL;
   struct timespec *T_tot = NULL;
   size_t *T_nr = NULL;
   struct timespec T_gtot[5];
   size_t T_gnr[5];
   memset( T_gtot, 0, sizeof(T_gtot));
   memset( T_gnr, 0, sizeof(T_gnr));
   struct timespec start;
   clock_gettime( CLOCK_MONOTONIC, &start );
   if (pid == 0)
   {
       ASSERT( args.input_size >= sizeof(params));
       ASSERT( args.output_size >= sizeof(params));
       memcpy( &params, args.input, sizeof(params));
       if (params.verbose)
          fprintf(stderr, "Launching probe...\n" );


       const char * checkpointName =
           params.checkpointName[0] == '\0'?NULL:params.checkpointName;

       if (checkpointName) {
           params_t p ;
           int e =
             readCheckpoint( checkpointName, &p, NULL, NULL, NULL, NULL, NULL);

           if (e) {
               if (params.verbose)
                 fprintf(stderr, "Checkpoint '%s' does not exist. "
                        "Starting from the beginning\n", checkpointName);
               params.checkpointLoad = 0;
           }
           else
           {
               if (params.verbose) {
                   int progress = 100.0 * p.i / (5 * p.M * p.K);
                   fprintf(stderr, "Loaded checkpoint '%s'; Continuing from "
                        "%2d %% progress\n", checkpointName, progress  );
               }
               params = p;
               params.checkpointLoad = 1;
           }
       }

       Ts = calloc( 5*params.M*params.K , sizeof(Ts[0]));
       T_avg = calloc( 5*params.M, sizeof(T_avg[0]));
       T_nr  = calloc( 5*params.M, sizeof(T_nr[0]));
       T_tot = calloc( 5*params.M, sizeof(T_tot[0]));


       if (params.checkpointLoad) {
           int e = readCheckpoint( checkpointName, &params, Ts, T_tot, T_nr, T_gtot, T_gnr);
           params.checkpointLoad = 1;

           if (e) {
               snprintf( params.message, sizeof(params.message),
                       "Checkpoint '%s' was corrupted. Please see whether you "
                       " can continue from '%s.bak' instead.\n",
                       checkpointName, checkpointName);
               memcpy( args.output, &params, sizeof(params));
               return;
           }

           if (params.nprocs != nprocs ) {
               snprintf( params.message, sizeof(params.message),
                       "Checkpoint '%s' was taken on run for %u processes. Now "
                       "only %u processes are available. Please change "
                       "the lpfrun parameters.\n", checkpointName,
                       (unsigned) params.nprocs, (unsigned) nprocs);
               memcpy( args.output, &params, sizeof(params));
               return;

           }
       }

       if (params.verbose) {
           double minSeconds = timespec_to_seconds(params.period)*params.K*params.M*5;
           size_t hours = floor(minSeconds / 3600.0);
           size_t minutes = floor( (minSeconds - 3600.0*hours) / 60.0 );
           double seconds = minSeconds - 3600.0*hours - 60.0 * minutes;
           fprintf(stderr, "Probing with:\n"
                  "   sample size = max(%zu, %ld.%09ld sec) x %zu x %zu > %zu hours %zu minutes %g seconds\n"
                  "   max volume per process = %zu x %u x %zu bytes = %g mbytes\n"
                  "   confidence requires = %g %%\n",
                  params.S, params.period.tv_sec, params.period.tv_nsec, params.K, params.M,
                  hours, minutes, seconds,
                  params.N, nprocs, params.wordSize, 1.0e-6 * params.N * nprocs * params.wordSize,
                  100.0 * params.CI );
       }
   }

   lpf_err_t rc = lpf_resize_memory_register( lpf, 5 );
   rc = rc == LPF_SUCCESS ? lpf_resize_message_queue( lpf, nprocs) : rc;
   if (rc != LPF_SUCCESS || (pid == 0 && (Ts == NULL || T_avg == NULL || T_nr == NULL || T_tot == NULL))
           || LPF_SUCCESS != lpf_sync( lpf, LPF_SYNC_DEFAULT)) {
       if (pid == 0) {
           snprintf( params.message, sizeof(params.message),
                   "Insufficient memory for initialisation\n");
           memcpy( args.output, &params, sizeof(params));
       }
       return;
   }

   // broadcast arguments
   lpf_memslot_t paramSlot = LPF_INVALID_MEMSLOT;
   lpf_register_global( lpf, &params, sizeof(params), &paramSlot );
   if (pid != 0 )
       lpf_get( lpf, 0, paramSlot, 0, paramSlot, 0, sizeof(params), LPF_MSG_DEFAULT );

   rc = lpf_sync( lpf, LPF_SYNC_DEFAULT );
   if (rc != LPF_SUCCESS)
   {
       if (pid == 0) {
           snprintf( params.message, sizeof(params.message),
                   "Unknown error\n");
           memcpy( args.output, &params, sizeof(params));
       }
       return;
   }

   const size_t S = params.S;
   const size_t N = params.N;
   const size_t M = params.M;
   const size_t K = params.K;
   const size_t wordSize = params.wordSize;
   const double CI = params.CI;
   const int verbose = params.verbose;
   const int checkpointLoad = params.checkpointLoad;
   const char * checkpointName = params.checkpointName[0] == '\0'?NULL:params.checkpointName;
   struct timespec checkpointInterval = params.checkpointInterval;
   struct timespec period = params.period;
   params.nprocs = nprocs;

   /* allocate H relation */
   rc = lpf_resize_message_queue( lpf, nprocs + N*nprocs );
   char * input = calloc( N*nprocs, wordSize);
   char * output = calloc( N*nprocs, wordSize);

   /* allocate buffer to hold permutation of tests */
   int8_t * choices = calloc( 5*M*K, sizeof(int8_t));
   size_t * buckets = calloc( 5*M*K, sizeof(size_t));

   // a variable to communicate a sudden stop.
   int stop = 0;

   if (rc != LPF_SUCCESS || input == NULL || output == NULL || choices == NULL
           || buckets == NULL
          ||  LPF_SUCCESS != lpf_sync( lpf, LPF_SYNC_DEFAULT ))
   {
       free(input);
       free(output);
       free(choices);
       free(buckets);
       if (pid == 0) {
           snprintf( params.message, sizeof(params.message),
                   "Insufficient memory for initialisation of %zu x %u messages of size %zu\n",
                   N, nprocs, wordSize );
           memcpy( args.output, &params, sizeof(params));
       }
       return;
   }

   /* init and register communication buffers */
   for (size_t i = 0; i < N*nprocs*wordSize; ++i)
   {
       input[i] = (char) i ;
       output[i] = (char) (~i);
   }

   lpf_memslot_t inputSlot, outputSlot, stopSlot, periodSlot;
   lpf_register_global( lpf, input, N*nprocs*wordSize, &inputSlot );
   lpf_register_global( lpf, output, N*nprocs*wordSize, &outputSlot );
   lpf_register_global( lpf, &stop, sizeof(stop), &stopSlot );
   lpf_register_global( lpf, &period, sizeof(period), &periodSlot );

   // WARM UP & and come up with a suitable time period
   { size_t n = 0;
     measure( lpf, pid, nprocs, inputSlot, outputSlot, &stop, stopSlot,
           period, 0,      wordSize, &n );
     measure( lpf, pid, nprocs, inputSlot, outputSlot, &stop, stopSlot,
           period, nprocs, wordSize, &n );
     measure( lpf, pid, nprocs, inputSlot, outputSlot, &stop, stopSlot,
           period, 2*nprocs, wordSize, &n );
     measure( lpf, pid, nprocs, inputSlot, outputSlot, &stop, stopSlot,
           period, N*nprocs,  wordSize, &n );

     // process zero broadcasts it
     lpf_sync( lpf, LPF_SYNC_DEFAULT );
     if (pid != 0)
         lpf_get( lpf, 0, periodSlot, 0, periodSlot, 0, sizeof(period), LPF_MSG_DEFAULT);
     lpf_sync( lpf, LPF_SYNC_DEFAULT );

     // increase sample period if more samples are required
     n=0;
     int doubling = 0;
     while(1) {
       measure( lpf, pid, nprocs, inputSlot, outputSlot, &stop, stopSlot,
               period, N*nprocs,  wordSize, &n );
       if (n >= S) break;
       period = timespec_double( period );
       doubling += 1;
     }
     if (!checkpointLoad)
     {
         // store it
         params.period = period;
         if (verbose && pid == 0 && doubling > 0) {
           double minSeconds = timespec_to_seconds(period)*K*M*5;
           size_t hours = floor(minSeconds / 3600.0);
           size_t minutes = floor( (minSeconds - 3600.0*hours) / 60.0 );
           double seconds = minSeconds - 3600.0*hours - 60.0 * minutes;
           fprintf(stderr, "Unit sampling period was extended to %ld.%09ld seconds.\n"
                           "=> new predicted duration is %zu hours, %zu minutes, and %g seconds \n",
                           period.tv_sec, period.tv_nsec,
                           hours, minutes, seconds);
         }
     }
   }

   if (checkpointLoad)
   {
       period = params.period;
       if (verbose && pid == 0) {
           double minSeconds = timespec_to_seconds(period)*(K*M*5-params.i);
           size_t hours = floor(minSeconds / 3600.0);
           size_t minutes = floor( (minSeconds - 3600.0*hours) / 60.0 );
           double seconds = minSeconds - 3600.0*hours - 60.0 * minutes;
           fprintf(stderr, "Continuing from checkpoint. sampling period is %ld.%09ld seconds.\n"
                           "=> predicted completion time is in about %zu hours, %zu minutes, and %g seconds \n",
                           period.tv_sec, period.tv_nsec,
                           hours, minutes, seconds);
       }
   }


   // GET RANDOM PERMUTATION OF M SAMPLES FOR EACH KIND
   for (int i = 0; i < 5; ++i) {
       for (size_t j = 0; j < M*K; ++j) {
           choices[i*M*K + j] = i;
           buckets[i*M*K + j] = j / K;
       }
   }

   unsigned int seed = 0;
   for (size_t i = 0; i < 5*M*K; ++i)
   {
       size_t j = i + 1.0*(5*M*K - i) * rand_r(&seed) / (1.0 + RAND_MAX);

       // swap choices[i] and choices[j]
       int8_t a = choices[i];
       choices[i] = choices[j];
       choices[j] = a;

       size_t b = buckets[i];
       buckets[i] = buckets[j];
       buckets[j] = b;
   }


   // DO MEASUREMENTS
   const int P=1, P2=2, PN=3, r=4;
   const size_t H[5] = { 0, nprocs, 2*nprocs, N*nprocs, N*nprocs};
   const int reportStep = 10;
   struct timespec t0_checkpoint;
   clock_gettime( CLOCK_MONOTONIC, &t0_checkpoint);

   if (pid == 0 && verbose)
       fprintf(stderr, "Reporting every %f %% progress...\n", 100.0 / reportStep );

   for ( size_t i = params.i; i < 5*M*K; ++i) {
       struct timespec t;
       size_t n = 0;
       int choice = choices[i];
       if (choice < 4)
           t = measure( lpf, pid, nprocs, inputSlot, outputSlot, &stop,
                             stopSlot, period, H[choice], wordSize, &n );
       else
           t = measure_memcpy( input, output, period, H[choice], wordSize, &n);

       size_t bucket = buckets[i];
       if (pid==0) {
          T_tot[choice*M+bucket] = timespec_add( T_tot[choice*M+bucket], t);
          T_nr[choice*M+bucket] += n;

          T_gtot[choice] = timespec_add( T_gtot[choice], t );
          T_gnr[choice] += n;

          Ts[i] = timespec_to_seconds(t) / n;

          if (verbose && i % sizeMax(1u, (5*M*K/reportStep)) == 0) {
              struct timespec stop;
              clock_gettime( CLOCK_MONOTONIC, &stop );
              double progress = 100.0 / (5*M*K) * i;
              double elapsed = timespec_to_seconds( timespec_diff(stop, start));
              fprintf(stderr, "Progress: %f %%  after %g seconds\n",
                      progress, elapsed );
          }

          struct timespec t1_checkpoint;
          clock_gettime( CLOCK_MONOTONIC, &t1_checkpoint );
          if ( checkpointName && timespec_cmp( checkpointInterval,
                      timespec_diff(t1_checkpoint, t0_checkpoint)) < 0)
          {
              params.i = i;
              clock_gettime( CLOCK_MONOTONIC, &t0_checkpoint );
              int e = writeCheckpoint(checkpointName, &params, Ts, T_tot ,T_nr, T_gtot, T_gnr);
              if (e)
                  fprintf(stderr, "Warning: unable to write checkpoint '%s'\n", checkpointName);
          }
       }
   }


   if (pid == 0) {
       if (verbose)
           fprintf(stderr, "Post processing...\n" );

       { params.i = 5*M*K;
         if (checkpointName) {
             int e = writeCheckpoint(checkpointName, &params, Ts, T_tot ,T_nr, T_gtot, T_gnr);
             if (e)
              fprintf(stderr, "Warning: could not write final checkpoint '%s'\n", checkpointName);
             else if (verbose)
              fprintf(stderr, "Wrote final checkpoint '%s'\n", checkpointName);
         }
       }

       // compute the averages over K samples
       for (size_t i = 0; i < 5*M; ++i) {
           T_avg[i] = timespec_to_seconds(T_tot[i]) / T_nr[i];
       }

       // compute global average and make a CDF for each for the measurement points
       double T_gavg[5];
       for (int i = 0; i < 5; ++i) {
           T_gavg[i] = timespec_to_seconds(T_gtot[i]) / T_gnr[i];
           qsort( T_avg + i*M, M, sizeof(T_avg[0]), cmpDouble );
       }

       // compute the sample global variance and third central moment
       double T_gvar[5];
       double T_gthird[5];
       memset( T_gvar, 0, sizeof(T_gvar) );
       memset( T_gthird, 0, sizeof(T_gthird) );
       for (size_t i = 0; i < 5*M*K; ++i)
       {
           double x = 1.0 / (M*K);
           double x1 = 1.0 / (M*K-1);
           double y = Ts[i] - T_gavg[choices[i]];
           T_gvar[choices[i]] += x1 * y * y;
           T_gthird[choices[i]] += x * y * y * y;
       }

       // compute the sample variance of T_avg
       double T_avg_var[5];
       memset( T_avg_var, 0, sizeof(T_avg_var));
       for (size_t i = 0; i < 5*M; ++i) {
           double x1 = 1.0 / (M-1);
           double y = T_avg[i] - T_gavg[i/M];
           T_avg_var[i/M] += x1 * y * y;
       }


       // compute BSP parameters
       params.g = (T_gavg[PN]- T_gavg[P2]) / (nprocs * (N-2));
       params.L = 2*T_gavg[P] - T_gavg[P2];
       params.T0 = T_gavg[0];
       params.r = T_gavg[r] / (N*nprocs);

       // estimate confidence interval
       double epsilon_CI[5]; // CI based on samples
       double epsilon_CLT[5]; // CI based on Central Limit Theorem
       for (int i = 0; i < 5; ++i) {
           size_t p0 = (0.5 + 0.5 *CI) * M;
           size_t p1 = (0.5 - 0.5 *CI) * M;
           double eps1 = fabs( T_avg[ i*M + p0 ] - T_gavg[i] );
           double eps2 = fabs( T_avg[ i*M + p1 ] - T_gavg[i] );
           epsilon_CI[i] = eps1 < eps2 ? eps2 : eps1;

           // if CLT holds for K samples, we could also have computed
           // the confidence interval this way
           double x = confIntervalNormal( CI, 1e-14);
           epsilon_CLT[i] = x * sqrt( T_gvar[i] / K );
       }

       params.eps_g = (epsilon_CI[PN] + epsilon_CI[P2]) / (nprocs * (N-2));
       params.eps_L = 2*epsilon_CI[P] + epsilon_CI[P2];
       params.eps_T0 = epsilon_CI[0];
       params.eps_r = epsilon_CI[r] / (nprocs * N);

       size_t msgOffset =
              snprintf(params.message, sizeof(params.message),
               "period = %ld.%09ld sec.\n"
                "T_0  = %g sec +/- %g (histogram) or %g (CLT). (%zu samples, %g variance, %g third central moment)\n"
                "T_P  = %g sec +/- %g (histogram) or %g (CLT). (%zu samples, %g variance, %g third central moment)\n"
                "T_2P = %g sec +/- %g (histogram) or %g (CLT). (%zu samples, %g variance, %g third central moment)\n"
                "T_NP = %g sec +/- %g (histogram) or %g (CLT). (%zu samples, %g variance, %g third central moment)\n"
                "T_r  = %g sec +/- %g (histogram) or %g (CLT). (%zu samples, %g variance, %g third central moment) \n"
                "g = %g sec/word +/- %g , L = max( %g +/- %g , %g +/- %g ) = %g sec, r = %g sec/word +/- %g\n",
                period.tv_sec, period.tv_nsec,
                T_gavg[0], epsilon_CI[0], epsilon_CLT[0], T_gnr[0], T_gvar[0], T_gthird[0],
                T_gavg[P], epsilon_CI[P], epsilon_CLT[P], T_gnr[P], T_gvar[P], T_gthird[P],
                T_gavg[P2], epsilon_CI[P2], epsilon_CLT[P2], T_gnr[P2], T_gvar[P2], T_gthird[P2],
                T_gavg[PN], epsilon_CI[PN], epsilon_CLT[PN], T_gnr[PN], T_gvar[PN], T_gthird[PN],
                T_gavg[r], epsilon_CI[r], epsilon_CLT[r], T_gnr[r], T_gvar[r], T_gthird[r],
                params.g, params.eps_g,
                params.L, params.eps_L,
                params.T0, params.eps_T0,
                params.L < params.T0 ? params.T0 : params.L,
                params.r, params.eps_r );


       // print the CDF of the averages
       const double p[] = {0.0, 0.005, 0.025, 0.05,
                          0.1, 0.5,  0.9,
                          0.95, 0.975, 0.995 };
       const size_t np = sizeof(p)/sizeof(p[0]);
       msgOffset +=
           snprintf( params.message + msgOffset ,
                    sizeof(params.message) - msgOffset,
                    "CDF (%%): ");
       for (size_t j = 0; j < np; ++j) {
           msgOffset +=
              snprintf( params.message + msgOffset ,
                     sizeof(params.message) - msgOffset,
                     "% 10g ", p[j] * 100.0);
       }
        msgOffset += snprintf( params.message + msgOffset ,
                     sizeof(params.message) - msgOffset, "     100\n" );
       for (int i = 0; i < 5; ++i) {
           const char * c_str[5] = { " 0", " P", "2P", "NP", " r" };
           msgOffset +=
               snprintf( params.message + msgOffset ,
                         sizeof(params.message) - msgOffset,
                         "T[%s] -> ", c_str[i]);
           for (size_t j = 0; j < np; ++j) {
               size_t k = sizeMin(M-1, round( M * p[j] ));
               msgOffset +=
                  snprintf( params.message + msgOffset ,
                         sizeof(params.message) - msgOffset,
                         "% 10.4g ", T_avg[i*M + k ] );

           }
           msgOffset +=
               snprintf( params.message + msgOffset,
                         sizeof(params.message) - msgOffset,
                         "% 10.4g  ( mean = %g , var = %g ) \n",
                         T_avg[i*M + M-1], T_gavg[i], T_avg_var[i] );
       }



       ASSERT( args.output_size >= sizeof(params));
       memcpy( args.output, &params, sizeof(params));
   }
   lpf_deregister( lpf, paramSlot );
   lpf_deregister( lpf, inputSlot );
   lpf_deregister( lpf, outputSlot );
   lpf_deregister( lpf, stopSlot );
   lpf_deregister( lpf, periodSlot );
   free(input);
   free(output);
   free(choices);
   free(buckets);
   free(Ts);
   free(T_nr);
   free(T_tot);
   free(T_avg);
}

int view_checkpoint(const char * checkpointName, params_t * output_params )
{
   params_t params;
   double *Ts = NULL;
   double *T_avg = NULL;
   struct timespec *T_tot = NULL;
   size_t *T_nr = NULL;
   struct timespec T_gtot[5];
   size_t T_gnr[5];
   const int P=1, P2=2, PN=3, r=4;
   memset( T_gtot, 0, sizeof(T_gtot));
   memset( T_gnr, 0, sizeof(T_gnr));

   if (! checkpointName ) {
       fprintf(stderr, "Checkpoint file must be given using the --checkpoint=<file> argument\n");
       return 1;
   }

   int e = readCheckpoint( checkpointName, &params, NULL, NULL, NULL, NULL, NULL);

   if (e) {
       fprintf(stderr, "Checkpoint '%s' does not exist. ",
               checkpointName );
       return 1;
   }
   else if (params.i < 5 * params.M * params.K)
   {
       int progress = 100.0 * params.i / (5 * params.M * params.K);
       fprintf(stderr, "Loaded checkpoint '%s', but measurements have not completed;\n "
                "Now at %2d %% progress\n", checkpointName, progress  );
       return 1;
   }

   Ts = calloc( 5*params.M*params.K , sizeof(Ts[0]));
   T_avg = calloc( 5*params.M, sizeof(T_avg[0]));
   T_nr  = calloc( 5*params.M, sizeof(T_nr[0]));
   T_tot = calloc( 5*params.M, sizeof(T_tot[0]));

   if (!Ts || !T_avg || !T_nr || !T_tot ) {
       fprintf(stderr, "Insufficient memory to load checkpoint '%s'\n", checkpointName );
       return 1;
   }

   e = readCheckpoint( checkpointName, &params, Ts, T_tot, T_nr, T_gtot, T_gnr);
   if (e) {
       snprintf( params.message, sizeof(params.message),
               "Checkpoint '%s' was corrupted. Please see whether you "
               " can load  '%s.bak' instead.\n",
               checkpointName, checkpointName);
       return 1;
   }

   const size_t N = params.N;
   const size_t M = params.M;
   const double CI = params.CI;
   struct timespec period = params.period;
   lpf_pid_t nprocs = params.nprocs;

   // FIXME: This post-processing code is duplicated from takeStats

   // compute the averages over K samples
   for (size_t i = 0; i < 5*M; ++i) {
       T_avg[i] = timespec_to_seconds(T_tot[i]) / T_nr[i];
   }

   // compute global average and make a CDF for each for the measurement points
   double T_gavg[5];
   for (int i = 0; i < 5; ++i) {
       T_gavg[i] = timespec_to_seconds(T_gtot[i]) / T_gnr[i];
       qsort( T_avg + i*M, M, sizeof(T_avg[0]), cmpDouble );
   }

   // compute the sample variance of T_avg
   double T_avg_var[5];
   memset( T_avg_var, 0, sizeof(T_avg_var));
   for (size_t i = 0; i < 5*M; ++i) {
       double x1 = 1.0 / (M-1);
       double y = T_avg[i] - T_gavg[i/M];
       T_avg_var[i/M] += x1 * y * y;
   }


   // compute BSP parameters
   params.g = (T_gavg[PN]- T_gavg[P2]) / (nprocs * (N-2));
   params.L = 2*T_gavg[P] - T_gavg[P2];
   params.T0 = T_gavg[0];
   params.r = T_gavg[r] / (N*nprocs);

   // estimate confidence interval
   double epsilon_CI[5]; // CI based on samples
   for (int i = 0; i < 5; ++i) {
       size_t p0 = (0.5 + 0.5 *CI) * M;
       size_t p1 = (0.5 - 0.5 *CI) * M;
       double eps1 = fabs( T_avg[ i*M + p0 ] - T_gavg[i] );
       double eps2 = fabs( T_avg[ i*M + p1 ] - T_gavg[i] );
       epsilon_CI[i] = eps1 < eps2 ? eps2 : eps1;
   }

   params.eps_g = (epsilon_CI[PN] + epsilon_CI[P2]) / (nprocs * (N-2));
   params.eps_L = 2*epsilon_CI[P] + epsilon_CI[P2];
   params.eps_T0 = epsilon_CI[0];
   params.eps_r = epsilon_CI[r] / (nprocs * N);

   printf( "period = %ld.%09ld sec.\n"
            "T_0  = %g sec +/- %g (histogram). (%zu samples)\n"
            "T_P  = %g sec +/- %g (histogram). (%zu samples)\n"
            "T_2P = %g sec +/- %g (histogram). (%zu samples)\n"
            "T_NP = %g sec +/- %g (histogram). (%zu samples)\n"
            "T_r  = %g sec +/- %g (histogram). (%zu samples) \n"
            "g = %g sec/word +/- %g , L = max( %g +/- %g , %g +/- %g ) = %g sec, r = %g sec/word +/- %g\n",
            period.tv_sec, period.tv_nsec,
            T_gavg[0], epsilon_CI[0], T_gnr[0],
            T_gavg[P], epsilon_CI[P], T_gnr[P],
            T_gavg[P2], epsilon_CI[P2], T_gnr[P2],
            T_gavg[PN], epsilon_CI[PN], T_gnr[PN],
            T_gavg[r], epsilon_CI[r], T_gnr[r],
            params.g, params.eps_g,
            params.L, params.eps_L,
            params.T0, params.eps_T0,
            params.L < params.T0 ? params.T0 : params.L,
            params.r, params.eps_r );


    // print the CDF of the averages
    const double p[] = {0.0, 0.005, 0.025, 0.05,
                      0.1, 0.5,  0.9,
                      0.95, 0.975, 0.995 };
    const size_t np = sizeof(p)/sizeof(p[0]);
    printf("CDF (%%): ");
    for (size_t j = 0; j < np; ++j)
       printf("% 10g ", p[j] * 100.0);

    printf( "     100\n" );
    for (int i = 0; i < 5; ++i) {
       const char * c_str[5] = { " 0", " P", "2P", "NP", " r" };
       printf( "T[%s] -> ", c_str[i]);
       for (size_t j = 0; j < np; ++j) {
           size_t k = sizeMin(M-1, round( M * p[j] ));
           printf( "% 10.4g ", T_avg[i*M + k ] );

       }
       printf( "% 10.4g  ( mean = %g , var = %g ) \n",
                T_avg[i*M + M-1], T_gavg[i], T_avg_var[i] );
    }
    *output_params = params;
    return 0;
}



const char * has_prefix( const char * str, const char * prefix)
{
    do {
        if (*prefix == '\0') return str;
        if (*str != *prefix ) return NULL;
        ++prefix;
        ++str;
    } while(1);
}

void help_params(void)
{
    printf("  --word-size=<bytes>         The number of bytes of the basic word size.\n"
           "\n"
           "  --unit-period-usec=<time>   The minimum number of microseconds that a basic\n"
           "                              sampling period must last.\n"
           "\n"
           "  --unit-sample-size=<number> The minimum number of samples in a basic\n"
           "                              sampling period.\n"
           "\n"
           "  --whole-sample-size=<number> The number of basic sampling periods that will\n"
           "                              constitute the whole sample\n"
           "\n"
           "  --number-of-samples=<number> The number of whole samples that should be taken\n"
           "                              to estimate the confidence interval\n"
           "\n"
           "  --maximum-volume=<bytes>    The number of bytes in the largest h-relation\n"
           "\n"
           "  --confidence=<probability>  The probability that the resulting confidence\n"
           "                              interval should have.\n"
           "\n"
           "  --checkpoint=<file>         File to use for checkpointing measurement.\n"
           "                              If the file already exists, measurements will\n"
           "                              resume from there.\n"
           "\n"
           "  --checkpoint-interval=<seconds>\n"
           "                              The wall clock time between each checkpoint\n"
           "\n"
           "  --view                      View the checkpoint file, given by --checkpoint.\n"
           "\n"
           "  --verbose                   Turn on verbose messages.\n"
           "\n");
}

void help(const char * command )
{
    printf("SYNOPSIS:\n"
           "    Probe the BSP machine parameters in absolute terms and relative to memcpy.\n "
           "\n"
           "USAGE:\n"
           "    lpfrun %s [--word-size=<bytes>] [--unit-period-usec=<time>]\n"
           "              [--unit-sample-size=<number>] [--whole-sample-size=<number>]\n"
           "              [--number-of-samples=<number>] [--maximum-volume=<bytes>]\n"
           "              [--confidence=<probability>] [--checkpoint=<file>]\n"
           "              [--checkpoint-interval=<seconds>] [--view] [--verbose]\n"
           "\n"
           "OPTIONS\n" ,
           command
          );
    help_params();
}

cmdline_params_t parseCmdParams( int argc, char ** argv  )
{
    (void) argc;
   const char * const command = argv[0];
   size_t wordSize = 8;
   size_t unitSampleSize = 10;
   size_t wholeSampleSize = 100;
   size_t numberOfSamples = 1000;
   size_t maxVol = 2500000*4;
   double CI = 0.95;
   int verbose = 0;
   int view = 0;
   const char * checkpoint = NULL;
   struct timespec checkpointInterval;
   checkpointInterval.tv_sec = 3600;
   checkpointInterval.tv_nsec = 0;

   struct timespec period;
   period.tv_sec = 0;
   period.tv_nsec = 1;

   lpf_err_t rc = LPF_SUCCESS;
   lpf_machine_t machine = LPF_INVALID_MACHINE;
   rc = lpf_probe( LPF_ROOT, &machine );
   if ( rc != LPF_SUCCESS ) {
       fprintf(stderr, "Could not probe BSP machine\n");
       exit(EXIT_FAILURE);
   }
   const lpf_pid_t nprocs = machine.p;

   /* parse command line params */
   for (char ** arg = argv+1; *arg != NULL; ++arg )
   {
       const char * value = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"

       if (value = has_prefix(*arg, "--word-size=")) {
            wordSize = atol(value);
            if (wordSize < 1 ) {
                fprintf(stderr, "word size must be a positive, non-zero integer\n");
                help(command);
                exit(EXIT_FAILURE);
            }
        }
        else if ( value = has_prefix(*arg, "--unit-period-usec=")) {
            size_t usec = atol(value);
            size_t million = 1000000;
            period.tv_sec = usec / million ;
            period.tv_nsec = 1000*(usec % million);
        }
        else if ( value = has_prefix(*arg, "--unit-sample-size=")) {
            unitSampleSize = atol( value );
            if (unitSampleSize < 1 ) {
                fprintf(stderr, "Unit sample size must be a positive, non-zero integer\n");
                help(command);
                exit(EXIT_FAILURE);
            }
        }
        else if ( value = has_prefix(*arg, "--whole-sample-size=") ) {
            wholeSampleSize = atol( value );
        }
        else if ( value = has_prefix(*arg, "--number-of-samples=") ) {
            numberOfSamples = atol( value );
        }
        else if ( value = has_prefix(*arg, "--maximum-volume=" )) {
            maxVol = atol( value );
        }
        else if ( value = has_prefix(*arg, "--view") ) {
            view = 1;
        }
        else if ( value = has_prefix(*arg, "--verbose") ) {
            verbose = 1;
        }
        else if ( value = has_prefix(*arg, "--confidence=") ) {
            CI = atof( value );
        }
        else if ( value = has_prefix(*arg, "--checkpoint=") ) {
            checkpoint = value;
        }
        else if ( value = has_prefix(*arg, "--checkpoint-interval=") ) {
            size_t sec = atol(value);
            checkpointInterval.tv_sec = sec ;
            checkpointInterval.tv_nsec = 0;
        }
        else if ( 0 == strcmp(*arg, "--help-params" )) {
            help_params();
            exit(EXIT_SUCCESS);
        }
        else if ( 0 == strcmp(*arg, "--help" )) {
            help(command);
            exit(EXIT_SUCCESS);
        }
        else {
            fprintf(stderr, "Unrecognized option '%s'\n", *arg);
            help(command);
            exit(EXIT_FAILURE);
        }
#pragma GCC diagnostic pop
   }

   cmdline_params_t result;
   memset(&result, 0, sizeof(result));
   result.S = unitSampleSize;
   result.K = wholeSampleSize / unitSampleSize;
   result.M = numberOfSamples;
   result.N = sizeMax( 3, maxVol / wordSize / nprocs );
   result.wordSize = wordSize;
   result.period = period;
   result.CI = CI;
   result.verbose = verbose;
   result.view = view;
   result.nprocs = nprocs;
   result.checkpointInterval = checkpointInterval;
   if (!checkpoint) {
       result.checkpointName[0]='\0';
   }
   else
   {
       strncpy( result.checkpointName, checkpoint,
               sizeof(result.checkpointName) );
       result.checkpointName[sizeof(result.checkpointName)-1]='\0';
   }
   return result;
}

int main( int argc, char ** argv)
{
   cmdline_params_t cmdline = parseCmdParams(argc, argv);

   double g=1, L=1, T0=1, maxL=1, r=1;
   double eps_g=HUGE_VAL, eps_L=HUGE_VAL, eps_T0=HUGE_VAL, eps_maxL=HUGE_VAL, eps_r=HUGE_VAL;

   const int verbose=cmdline.verbose;

   params_t input_params, output_params;
   memset( &input_params, 0, sizeof(input_params));
   memset( &output_params, 0, sizeof(output_params));

   if ( cmdline.view ) {
       if ( view_checkpoint( cmdline.checkpointName, &output_params ) )
       {
           fprintf(stderr, "Failure reading checkpoint file\n" );
           return EXIT_FAILURE;
       }
   }
   else {
       input_params.S = cmdline.S;
       input_params.N = cmdline.N;
       input_params.M = cmdline.M;
       input_params.K = cmdline.K;
       input_params.wordSize = cmdline.wordSize;
       input_params.CI = cmdline.CI;
       input_params.verbose = cmdline.verbose;
       input_params.period = cmdline.period;
       input_params.checkpointInterval = cmdline.checkpointInterval;
       memcpy(input_params.checkpointName, cmdline.checkpointName,
               sizeof(input_params.checkpointName));
       lpf_args_t args;
       args.input = &input_params;
       args.input_size = sizeof(input_params);
       args.output = &output_params;
       args.output_size = sizeof(output_params);
       args.f_size =0;
       args.f_symbols = NULL;
       lpf_err_t rc = lpf_exec( LPF_ROOT, LPF_MAX_P, takeStats, args );
       if (rc != LPF_SUCCESS)
       {
           (void) fprintf(stderr, "Error: %s\n", output_params.message );
           return EXIT_FAILURE;
       }
   }

   g = output_params.g;
   eps_g = output_params.eps_g;
   L = output_params.L;
   eps_L = output_params.eps_L;
   T0 = output_params.T0;
   eps_T0 = output_params.eps_T0;
   r = output_params.r;
   eps_r = output_params.eps_r;

   if (L < T0) {
       maxL = T0;
       eps_maxL = eps_T0;
   }
   else
   {
       maxL = L;
       eps_maxL = eps_L;
   }

   const size_t W = output_params.wordSize;
   const lpf_pid_t P = output_params.nprocs;
   const size_t N = output_params.N;
   const size_t K = output_params.K;
   const size_t M = output_params.M;
   const double CI = output_params.CI;
   const double usec = 1.0e+6 * timespec_to_seconds( output_params.period );
   if (verbose) {
       fprintf(stderr, "Note: %s\n", output_params.message );
   }
   printf("=============================================================\n");
   printf("Processors:           %7u\n", P);
   printf("Word Size:            %7zu bytes\n", W);
   printf("Max volume:           %7zu bytes\n", N*W*P );
   printf("Number of samples     %7.5g micro sec. x %7zu x %7zu samples\n", usec, K, M);
   printf("Confidence            % 7g %% \n", CI*100.0);
   printf("\n--------------------------------------------------------------\n");
   if ( maxL > eps_maxL)
       printf("Barrier Latency:      %7.5g +/- %g us = max( %7.5g +/- %g, %7.5g +/- %g ) us\n",
              maxL*1e+6, eps_maxL * 1e+6, L*1e+6, eps_L * 1e+6, T0*1e+6, eps_T0*1e+6);
   else
       printf("Barrier Latency: (IRREGULAR) %7.5g us = max (%7.5g , %7.5g) us\n",
               maxL * 1e+6, L*1e+6, T0*1e+6);

   if ( g > eps_g )
       printf("Non-local Throughput: %7.5g +/- %g MB/sec\n",
               1e-6 * W/g, 1e-6*W*(1.0/(g-eps_g) - 1.0/g));
   else
       printf("Non-local Throughput: (IRREGULAR) %7.5g MB/sec\n",
               1e-6 * W/g);


   if ( r > eps_r )
       printf("Memcpy speed (r):     %7.5g +/- %g MB/sec\n",
               1e-6 * W/r, 1e-6*W*(1.0/(r-eps_r) - 1.0/r));
   else
       printf("Memcpy speed (r): (IRREGULAR) %7.5g MB/sec\n",
               1e-6 * W/r );

   printf("\n------ AVERAGE BSP PARAMETERS --------------------------------\n");
   printf("Absolute:            g = %7.5g +/- %g sec/byte, L = %7.5g +/- %g sec\n",
           g/W, eps_g/W, maxL, eps_maxL);
   printf("Normalized (memcpy): g = %7.5g +/- %g x,         L = %7.5g +/- %g words, r = %7.5g +/- %g sec/byte\n",
           g/r, eps_g/(r-eps_r), maxL/r, eps_maxL / (r - eps_r), r/W, eps_r/W);
   printf("==============================================================\n");
   return EXIT_SUCCESS;
}

