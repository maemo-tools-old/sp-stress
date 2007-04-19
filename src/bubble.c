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
