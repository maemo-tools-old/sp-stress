/* This file is part of sp-stress
 *
 * Copyright (C) 2006,2008 Nokia Corporation. 
 *
 * Contact: Eero Tamminen <eero.tamminen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License 
 * version 2 as published by the Free Software Foundation. 
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */


/* ========================================================================= *
 * File: cpuload.c
 *
 * Author: Leonid Moiseichuk
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
#include <linux/sched.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define FALSE 0
#define TRUE 1

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
 * Set nice value
 * ========================================================================= */
static int set_nice(int change)
{
   /* Nice() may return -1 even if it succeeded,
    * thus need to check errno
    */
   errno = 0;
   if (nice(19) < 0 && errno != 0)
     perror ("\nWARNING: setting priority failed: operating with default.\nReason");
   return TRUE;
}

/* ========================================================================= *
 * Set process scheduler
 * ========================================================================= */
static int set_sched(int policy, int load)
{
   struct sched_param param;
   
   if ((policy == SCHED_RR || policy == SCHED_FIFO)
       && (load == 0 || load > 90))
   {
      fprintf(stderr, "\nERROR: unsuitable %d load (random or >90) selected for real-time scheduling.\n", load);
      return FALSE;
   }
   
   memset(&param, 0, sizeof(param));
   /* use the highest priority for given scheduler policy */
   param.sched_priority = sched_get_priority_max(policy);
   if (sched_setscheduler(0, policy, &param) < 0)
     perror("\nWARNING: setting scheduler failed failed: operating with default.\nReason");

   return TRUE;
}

/* ========================================================================= *
 *Argument parsing, return FALSE for failure
 * ========================================================================= */
static int parse_args(int argc, const char* argv[], unsigned *load)
{
   char sched_pol;
   
   if (argc < 2 || argc > 4)
     return FALSE;

   *load = strtoul(argv[argc-1], NULL, 0);

   if (argc == 2)
     return TRUE;

   /* backwards compatibility */
   if (argc == 3)
   {
      /* nice to highest priority? */
      if (!strncmp(argv[1], "-p", 2))
	return set_nice(-19);
      return FALSE;
   }
   
   /* not "-s ."? */
   sched_pol = argv[2][0];
   if (argv[1][0] != '-' || argv[1][1] != 's' || argv[1][2]
       || !sched_pol || argv[2][1])
     return FALSE;


   /* which scheduling policy requested? */
   switch(tolower(sched_pol))
   {
      /* nice to lowest priority */
   case 'l':
      return set_nice(19);
      /* nice to highest priority */
   case 'h':
      return set_nice(-19);
      /* batch scheduler */
   case 'b':
      return set_sched(SCHED_BATCH, *load);
      /* scheduler that goes over default SCHED_OTHER */
   case 'f':
      return set_sched(SCHED_FIFO, *load);
      /* SCHED_FIFO with round robin policy */
   case 'r':
      return set_sched(SCHED_RR, *load);
      /* default scheduler */
   case 'o':
      return set_sched(SCHED_OTHER, *load);
   default:
      fprintf(stderr, "\nERROR: Unknown scheduling policy / priority '%c'.\n", sched_pol);
      return FALSE;
   }
}

/* ========================================================================= *
 * Main function of CPU load generator.
 * ========================================================================= */

int main(int argc, const char* argv[])
{
   const char *name;
   unsigned int load = 0;

   printf ("\nCPU load generator, build %s %s.\n", __DATE__, __TIME__);
   printf ("Copyright (C) 2006,2008 Nokia Corporation.\n");
   
   if (parse_args(argc, argv, &load))
   {
      calibrate_cpu();
      generate_load(load);
      return 0;
   }
   /* basename */
   name = strrchr(argv[0], '/');
   if (name)
     name++;
   else
     name = argv[0];
   /* usage */
   printf("\nUsage: %s [-s <id>] <highest CPU load>\n"
	  "\nExample: %s -s h 50\n\n", name, name);
   printf("CPU load of 0 means random load, anything else is percentage (1-100).\n"
	  "\nThe value given to '-s' can be used to set the scheduling priority/policy:\n"
	  "\tl -- lowest nice() priority\n"
	  "\th -- highest nice() priority\n"
	  "\tb -- use SCHED_BATCH scheduler\n"
	  "\tf -- use SCHED_FIFO scheduler (real time)\n"
	  "\tr -- use SCHED_RR scheduler (real time)\n"
	  "\to -- use SCHED_OTHER (default) scheduler\n"
	  "\nSee \"man sched_setscheduler\" and \"man 2 nice\".\n");
   return 1;
}
