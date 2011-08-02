/* This file is part of sp-stress
 *
 * Copyright (C) 2006,2009,2011 Nokia Corporation.
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
 * 25-Jul-2011 Leonid Moiseichuk
 * - add -f option to fill as "rand" | "fast"
 * - add -j option to handle OOM_ADJ
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

/* Filling pages approach */
enum FILL
{
  FILL_FAST, FILL_RAND
};


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

static int open_oom_adj(void)
{
  return open("/proc/self/oom_adj", O_RDWR);
}

static int get_oom_adj(void)
{
  int val = 0;
  int fd = open_oom_adj();
  if (fd >= 0)
  {
    char buf[64];
    if (read(fd, buf, sizeof(buf)) > 0)
      val = atoi(buf);
    close(fd);
  }

  return val;
} /* get_oom_adj */

static int set_oom_adj(int oom_adj)
{
  int fd = open_oom_adj();
  int len= 0;
  if (fd >= 0)
  {
    char buf[64];
    len = snprintf(buf, sizeof(buf), "%d", oom_adj);
    len = write(fd, buf, len);
    close(fd);
  }

  return (len > 0);
} /* set_oom_adj */


static int usage(const char *progname)
{
  printf ("\nUsage: %s [ -e ] [-f fast|rand] [-j <oom_adj>|inherit] [ -l ] <megabytes>\n", progname);
  printf ("\nOptions:\n");
  printf ("  -e\t\texit after consuming/dirtying the allocated memory.\n");
  printf ("  -l\t\tthe given amount of RAM is left free instead of consumed.\n");
  printf ("  -f\t\tfilling memory using 'rand' or 'fast' method.\n");
  printf ("  -j\t\tset oom_adj to specified value (default = 0) or inherit it.\n");
  printf ("\nExample:\n");
  printf ("  %s -e 20\n", progname);
  printf ("  %s -f fast -j inherit 20\n", progname);
  printf ("  %s -f fast -j -17 -l 20\n", progname);
  printf ("\n");
  return 1;
}

int main(int argc, char * const argv[])
{
   int c;
   opterr = 0;
   unsigned size = 0;
   unsigned leave_free;
   int exit_when_done = 0;
   int cur_oom = get_oom_adj();
   int new_oom = 0;
   void* data;
   enum FILL fill = FILL_RAND;

   if (argc < 2)
     return usage(argv[0]);

   while ((c = getopt(argc, argv, "el:f:j:")) != -1)
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
       case 'f':
          fill = (0 == strcmp(optarg, "fast") ? FILL_FAST : FILL_RAND);
          break;
       case 'j':
          new_oom = (optarg && 0 == strcmp(optarg, "inherit") ? cur_oom : atoi(optarg));
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

  printf ("current oom_adj is set to %d\n", cur_oom);
  if (cur_oom != new_oom)
  {
    printf ("updating oom_adj to %d: %s\n", new_oom, (set_oom_adj(new_oom) ? "AJDUSTED" : "FAILED"));
  }

  printf ("preparing data using %s filling method\n", (FILL_FAST == fill ? "FAST" : "RAND"));
  data = malloc(size);
  if ( data )
  {
    if (FILL_FAST == fill)
    {
      memset(data, size, size | 0x55);
    }
    else
    {
      long int* r = (long int*)data;
      unsigned  s = size / sizeof(*r);
      while (s-- > 0)
      {
        *r++ = random();
      }
    }
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
