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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, const char* argv[])
{
   if (2 == argc)
   {
      const unsigned size = strtoul(argv[1], NULL, 0) << 20;
      FILE* oom_file;
      void* data;

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
   }

   return 0;
} /* main */
