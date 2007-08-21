/* This file is part of sp-stress
 *
 * Copyright (C) 2006 Nokia Corporation. 
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
 * File: ioload.c
 *
 * Author: Leonid Moiseichuk
 *
 * Description:
 *    Producing stress IO sequence for specified file.
 *
 * History:
 *
 * 02-Jun-2006 Leonid Moiseichuk
 * - initial version created.
 * ========================================================================= */

/* ========================================================================= *
 * Includes
 * ========================================================================= */

#define __USE_GNU

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ========================================================================= *
 * Definitions.
 * ========================================================================= */

/* Division with rounding up */
#define DIV(val,div)    (((val) + ((div) >> 1)) / (div))

/* Read struct timespec in microseconds */
#define TS_TO_USECS(ts)  ((ts).tv_sec * 1000000u + DIV((ts).tv_nsec,1000u))

/* Size of test block, items */
#define BLOCK_SIZE      (4096 / sizeof(unsigned))

/* Test block for write and read */
typedef struct
{
   int data[BLOCK_SIZE];   /* First item is a random, every next more than first by 1 */
} BLOCK;

#define REPORT_COUNT    1000


/* ========================================================================= *
 * Local methods.
 * ========================================================================= */

static int test_block_setup(BLOCK* block)
{
   unsigned index;
   int* data = block->data;

   data[0] = rand();
   for (index = 1; index < BLOCK_SIZE; index++)
      data[index] = data[index - 1] + 1;

   return (0 == msync(block, sizeof(block), MS_SYNC));
} /* test_block_setup */

static void test_setup(BLOCK* blocks, unsigned counter)
{
   unsigned index;

   printf ("initialize %u blocks at 0x%08x\n", counter, (unsigned)blocks);
   for (index = 0; index < counter; index++)
   {
      BLOCK* b = blocks + index;

      if ( !test_block_setup(b) )
         printf ("*** block %u failed during initialization ***\n", index);
   }
} /* test_setup */

static void testing(BLOCK* blocks, unsigned counter)
{
   const time_t tstart= time(NULL);
   unsigned cycle = counter; /* once we did it during initialization */

   /* do it forever */
   while (1)
   {
      struct timespec beg_ts;
      struct timespec end_ts;
      unsigned n_reads  = 0;
      unsigned n_writes = 0;
      unsigned tm, h, m, s;

      clock_gettime(CLOCK_REALTIME, &beg_ts);
      while ((n_reads + n_writes) < REPORT_COUNT)
      {
         unsigned index = rand() % counter;
         BLOCK*   block = blocks + index;

         /* Calculate operation - read or write */
         if (0 == (rand() & 1))
         {
            int   ival = 0;
            const int* pval = block->data;

            while (ival < (int)BLOCK_SIZE)
            {
               /* Make a dummy read */
               if (ival == *pval++)
                  ((void)0);
               ival++;
            }

            n_reads++;
         }
         else
         {
            int  ival = 0;
            int* pval = block->data;

            while (ival < (int)BLOCK_SIZE)
            {
               /* Make a dummy write */
               *pval++ = ival++;
            }

            if (0 != msync(block, sizeof(block), MS_SYNC))
               printf ("*** block %u write failure ***\n", index);

            n_writes++;
         }
      }
      clock_gettime(CLOCK_REALTIME, &end_ts);
      cycle += REPORT_COUNT;

      tm  = time(NULL) - tstart;
      s   = tm % 60;
      tm /= 60;
      m   = tm % 60;
      h   = tm / 60;

      printf ("%02u:%02u:%02u: %u reads %u writes in test cycle %u done in %lu milliseconds\n",
               h, m, s, n_reads, n_writes, cycle, DIV(TS_TO_USECS(end_ts) - TS_TO_USECS(beg_ts), 1000));
   } /* while */
} /* testing */

static void force_stop(int signal)
{
   printf ("\n%d signal detected, testing terminated\n", signal);
   exit(0);
} /* force_stop */


/* ========================================================================= *
 * Methods.
 * ========================================================================= */

int main(const int argc, const char* argv[])
{
   int fd;
   unsigned counter;
   struct stat st;
   BLOCK* blocks;

   fprintf (stderr, "stress io. build %s %s.\n", __DATE__, __TIME__);

   if (argc != 2)
   {
      fprintf (stderr, "usage: %s workload_file\n", argv[0]);
      return -1;
   }

   fd = open(argv[1], O_SYNC|O_RDWR);
   if (fd < 0)
   {
      fprintf (stderr, "file %s opening failure %d: %s\n", argv[1], errno, strerror(errno));
      return -1;
   }

   if ( fstat(fd, &st) )
   {
      fprintf (stderr, "unable to get file %s information\n", argv[1]);
      return -1;
   }

   counter = st.st_size / sizeof(BLOCK);

   blocks = (BLOCK*)mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   if (MAP_FAILED == blocks)
   {
      fprintf (stderr, "file %s mapping failure %d: %s\n", argv[1], errno, strerror(errno));
      return -1;
   }

   printf ("file %s opened, %u blocks detected and mapped to 0x%08x\n", argv[1], counter, (unsigned)blocks);
   printf ("io started, to stop press ctrl+c\n");
   signal(SIGTERM, force_stop);

   test_setup(blocks, counter);
   testing(blocks, counter);

   return 0;
} /* main */

/* ---------------------------[ end of file ioload.c ]-------------------------- */

