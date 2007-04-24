/* ========================================================================= *
 * File: cpuload.c
 *
 * Copyright (C) 2006 Nokia. All rights reserved.
 *
 * Author: Leonid Moiseichuk <leonid.moiseichuk@nokia.com>
 *
 * Description:
 *    Generate necessary cpu load according to passed parameters.
 *
 * History:
 *
 * 28-Sep-2006 Leonid Moiseichuk
 * - initial version created.
 * ========================================================================= */

/* ========================================================================= *
 * Includes
 * ========================================================================= */

#define _XOPEN_SOURCE 500

#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* ========================================================================= *
 * Definitions.
 * ========================================================================= */

#define  CALIBRATION_SLICE    1.0   /* load slice value for calibration and load */
#define  CALIBRATION_PERIOD   5     /* seconds is minimal acceptable period      */
typedef unsigned long long LOOPS;

/* ========================================================================= *
 * Local data.
 * ========================================================================= */

static double s_slice = CALIBRATION_SLICE;
static LOOPS  s_loops = 0;   /* Number of empty loops per second that CPU can make */

/* ========================================================================= *
 * Methods.
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * cpu_load_slice -- Method to produce minimal CPU load slice
 * parameters: nothing.
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void cpu_load_slice(void)
{
   double load = 1.0;
   while (load < s_slice)
      load += 1.0;
} /* cpu_load_slice */


/* ------------------------------------------------------------------------- *
 * calibrate_cpu -- Detects CPU speed and estimate how much busy loops
 *    can we done per second.
 * parameters: nothing.
 * returns: nothing (sets s_loops).
 * ------------------------------------------------------------------------- */

static void calibrate_cpu(void)
{
   LOOPS   loops;
   time_t period;

   printf ("calibrating cpu speed:");
   fflush(stdout);

   do
   {
      loops = 1000 * 1000;

      while (loops)
      {
         LOOPS loop = 0;

         period = time(NULL);
         while (loop < loops)
         {
            cpu_load_slice();
            loop++;
         }
         period = time(NULL) - period;

         if (period >= CALIBRATION_PERIOD)
            break;
         else if ( 0 == period )
            loops *= 10;
         else
            loops *= 1 + CALIBRATION_PERIOD / period;
      }

      if ( loops )
         break;
      else
         s_slice *= 10.0;
   } while ( 1 );

   s_loops = loops / period;
   printf (" %llu loops per second\n", s_loops);
} /* calibrate_cpu */

/* ------------------------------------------------------------------------- *
 * generate_load -- Generates CPU load by using busy loops and usleep moments.
 * parameters: approximate load in percents.
 * returns: nothing (never returns).
 * ------------------------------------------------------------------------- */

static void generate_load(unsigned load)
{
   const LOOPS slice = s_loops / 100;
   static const char show[] = "-\\|/";
   unsigned stage = 0;

   printf ("generate %u%c cpu load\n", load, '%');
   while (1)
   {
      unsigned busy = (load ? load : (0 == (random() & 1) ? 100 : 50));
      unsigned idle = 100 - busy;

      printf("\r%c", show[stage]);
      fflush(stdout);
      if ( !show[++stage] )
         stage = 0;

      while (busy || idle)
      {
         if ( busy )
         {
            /* try to be produce load for 10 ms */
            LOOPS loop = 0;
            while (loop < slice)
            {
               cpu_load_slice();
               loop++;
            }
            busy--;
         }

         if ( idle )
         {
            /* sleeping for 10 ms */
            usleep(10 * 1000);
            idle--;
         }
      }
   }
} /* generate_load */

/* ========================================================================= *
 * Main function of CPU load generator.
 * ========================================================================= */

int main(int argc, const char* argv[])
{
   const unsigned load = (2 == argc ? strtoul(argv[1], NULL, 0) : 0);

   printf ("cpu load generator, build %s %s\n", __DATE__, __TIME__);
   printf ("2006 (c) Nokia\n");
   if (2 == argc)
   {
      if ( nice(-19) < 0 )
         printf ("set highest priority failed: operating with default\n");

      calibrate_cpu();
      generate_load(load);
   }
   else
   {
      printf ("usage: %s HIGHEST_CPU_LOAD_PERCENTAGE [0 means random load]\n", argv[0]);
   }

   return 0;
}
