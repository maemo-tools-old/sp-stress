/* This file is part of sp-stress
 *
 * Copyright (C) 2006,2009 Nokia Corporation. 
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
 * 02-Sep-2009 Eero Tamminen
 * - use open/write/close for oom_adj, fputs gets errors only at fclose.
 * 03-Sep-2009 Eero Tamminen
 * - add -e option for exiting after allocated memory is dirtied.
 * - no options gives usage, better size error handling.
 * ========================================================================= */

/* ========================================================================= *
 * Includes
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MINFO_MEMFREE "MemFree:"
#define MINFO_BUFFERS "Buffers:"
#define MINFO_CACHED  "Cached:"
#define MINFO_SWAPTOT "SwapTotal:"

#define MINFO_MEMFREE_LEN 9
#define MINFO_BUFFERS_LEN 9
#define MINFO_CACHED_LEN  8
#define MINFO_SWAPTOT_LEN 11


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

static int usage(const char *progname)
{
  printf ("\nUsage: %s [ -e ] [ -l ] <megabytes>\nn", progname);
  printf ("\nOptions:\n");
  printf ("  -e\t\texit after consuming/dirtying the allocated memory.\n");
  printf ("  -l\t\tthe given amount of RAM is left free instead of consumed.\n");
  printf ("\nExample:\n");
  printf ("  %s -e 20\n\n", progname);
  return 1;
}

int main(int argc, char * const argv[])
{
   int c;
   opterr = 0;
   unsigned size = 0;
   unsigned leave_free;
   int exit_when_done = 0;
   int oom_fd;
   void* data;

   if (argc < 2)
     return usage(argv[0]);

   while ((c = getopt(argc, argv, "el:")) != -1)
   {
     switch(c)
     {
       case 'e':
	 exit_when_done = 1;
	 break;
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
       default:
         return usage(argv[0]);
     }	
   }


   /* The traditional mode */
   if (!size)
   {
      if (optind >= argc)
        return usage(argv[0]);
      size = atoi(argv[optind]) << 20;
      if (size <= 0)
        return usage(argv[0]);
   }

      oom_fd = open("/proc/self/oom_adj", O_WRONLY);
      if (oom_fd >= 0 && write(oom_fd, "-17", 3) == 3)
      {
         printf ("oom scope adjusted\n");
      }
      else
      {
         printf ("oom scope NOT adjusted\n");
      }
      close(oom_fd);

      data = malloc(size);
      if ( data )
      {
         memset(data, size, size | 0x55);
         printf ("%u MB eat\n", size >> 20);
	 if (exit_when_done)
            return 0;
         while (1)
            sleep(60);
      }
      else
      {
         printf ("jammed with %u MB\n", size >> 20);
      }

   return 0;
} /* main */
