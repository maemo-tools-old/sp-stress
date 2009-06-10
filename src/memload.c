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
 * File: memload.c
 *
 * Author: Leonid Moiseichuk
 *
 * Description:
 *    Consume memory according to passed parameter.
 *
 * History:
 *
 * 28-Sep-2006 Leonid Moiseichuk
 * - initial version created.
 * ========================================================================= */

/* ========================================================================= *
 * Includes
 * ========================================================================= */

#define MINFO_MEMFREE "MemFree:"
#define MINFO_BUFFERS "Buffers:"
#define MINFO_CACHED  "Cached:"
#define MINFO_SWAPTOT "SwapTotal:"

#define MINFO_MEMFREE_LEN 9
#define MINFO_BUFFERS_LEN 9
#define MINFO_CACHED_LEN  8
#define MINFO_SWAPTOT_LEN 11

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static unsigned calc_allocsize(const unsigned leave_free)
{
  unsigned memfree = 0, buffers = 0, cached = 0;
  FILE *meminfo = fopen("/proc/meminfo", "r");
  
  if (meminfo)
  {
    /* 128 bytes is somewhat longer linebuffer than meminfo can currently
       output, but being prepared for format changes shouldn't hurt. */
    char line[128];

    while (fgets(line, sizeof(line), meminfo))
    {
      if (line == strstr(line, MINFO_MEMFREE))
      {
        memfree = (unsigned)strtoul(line+9, NULL, 0);
      }
      else if (line == strstr(line, MINFO_CACHED))
      {
        cached = (unsigned)strtoul(line+MINFO_CACHED_LEN, NULL, 0);
      }
      else if (line == strstr(line, MINFO_BUFFERS))
      {
        buffers = (unsigned)strtoul(line + MINFO_BUFFERS_LEN, NULL, 0);
      }
      else if (line == strstr(line, MINFO_SWAPTOT) &&
              ( (unsigned)strtoul(line + MINFO_SWAPTOT_LEN, NULL, 0) > 0))
      {
        printf ("Warning: Swap detected!\n");
        printf ("This might (or might not, depending on the case) cause\n");
        printf ("difference in behavior.\n");
      }
    }
  }
  else
  {
    printf ("Error getting memory statistics!\n");
    return 0;
  }

  fclose(meminfo);
  if ( (leave_free >> 20) > (memfree+buffers+cached)/1024 )
  {
    return 0;
  }
  else return (memfree+buffers+cached)/1024-(leave_free >> 20);
}

int main(int argc, char * const argv[])
{
   int c;
   opterr = 0;
   unsigned size = 0;
   unsigned leave_free;
   FILE* oom_file;
   void* data;

   while ((c = getopt(argc, argv, "l:")) != -1)
   {
     switch(c)
     {
       case 'l':
         leave_free = strtoul(optarg, NULL, 0) << 20;
         printf ("Should leave %u MB free\n", leave_free >> 20 );
         size = calc_allocsize(leave_free) << 20;
         if (size == 0)
         {
            printf("Can't do this (too much memory already in use?)\n");
            return 1;
         }
         break;
       case '?':
         printf ("Usage: %s [ -l ] MB\n", argv[0]);
         printf ("\nOptions:\n");
         printf ("  -l\t\tthe given amount of RAM is left free instead of consumed.\n");
         return 1;
       default:
         break;
     }	
   }


   /* The traditional mode */

   if (size == 0 && argc == 2)
   { 
     size = strtoul(argv[1], NULL, 0) << 20;
   }

      oom_file = fopen("/proc/self/oom_adj", "w");
      if (oom_file && fputs("-17", oom_file) > 0)
      {
         printf ("oom scope adjusted\n");
      }
      else
      {
         printf ("oom scope NOT adjusted\n");
      }
      fclose(oom_file);

      data = malloc(size);
      if ( data )
      {
         memset(data, size, size | 0x55);
         printf ("%u MB eat\n", size >> 20);
         while (1)
            sleep(60);
      }
      else
      {
         printf ("jammed with %u MB\n", size >> 20);
      }

   return 0;
} /* main */
