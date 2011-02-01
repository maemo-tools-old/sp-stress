/* ========================================================================= *
 * File: swpload.c
 *
 * This file is part of sp-stress.
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Leonid Moiseichuk <leonid.moiseichuk@nokia.com>
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
 * Description:
 *    Stress paging system load test. This application occupis required
 "    amount of memory and makes acceess for reading and updating pages to
 *    generate load for virtual memory and swapping.
 *
 *    The test application can be invoked using the following mandatory
 *    parameters in the command line:
 *    - clients  - number of clients to be executed in parralel
 *    - size     - size of workset for each client, megabytes
 *    - duration - test duration in seconds
 *    - type     - test type, could be combination for client activation
 *               and accessed pages pattern, main controls
 *        L - linear round robin access, 0..X
 *        R - random access, somewhere in between 0..X
 *        P - pseudo-random, from current point +- 10% of 0..X distance, if
 *            X is below 10 than next point will be -1, 0 or +1 from the current.
 *
 *    Examples:
 *      swpload 8 128 120 LL - 8 clients, 128 MB per each, 120 seconds, lin/lin access
 *      swpload 8 256 120 RP - 8 clients, 128 MB per each, 120 seconds,
 *        random client selection, pseudo-random pages access
 *
 *    See usage() output to be informed in the latest changes.
 *
 * History:
 *
 * 23-Jan-2009 Leonid Moiseichuk
 * - quality becomes testable, a bit changed output.
 *
 * 19-Jan-2009 Leonid Moiseichuk
 * - initial version of created.
 * ========================================================================= */

/* ========================================================================= *
 * Includes
 * ========================================================================= */

#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ========================================================================= *
 * General settings.
 * ========================================================================= */

#define SL_PSEUDO_RANDOM  10        /* How much percents shall we go up or down     */
#define SL_SIGTIME        SIGALRM   /* Testing done, no more execution is expected  */
#define SL_SIGDONE        SIGTERM   /* Testing done, no more execution is expected  */
#define SL_SIGINFO        SIGUSR1   /* Which signal used in for information excange */

#define SL_CAPACITY(a)    (sizeof(a) / sizeof(*a))

/* ========================================================================= *
 * Definitions.
 * ========================================================================= */

typedef enum
{
  SL_Unknown,         /* Working mode is not known */
  SL_Linear,          /* Working mode is linear    */
  SL_Random,          /* Working mode is random    */
  SL_PseudoRandom     /* Working mode is pseudo-random, see the SL_PSEUDO_RANDOM */
} SL_MODE;


typedef struct
{
  unsigned  clients;  /* Number of clients in this test cycle           */
  unsigned  workset;  /* Number of pages in working set for testing     */
  time_t    t_limit;  /* Time when we shall stop or 0 if unlimited mode */
  SL_MODE   cl_mode;  /* Mode to choose the next client to be iterated  */
  SL_MODE   pg_mode;  /* Mode to choose the next page to be accessed    */
  pid_t*    pids;     /* Process IDs for all clients                    */
}  SL_OPTS;

/* ========================================================================= *
 * Common Local data.
 * ========================================================================= */

static SL_OPTS  opts;       /* Program options which are shared between this session    */
static volatile int info_flag; /* Flag to indicate that signal is received for sl_wait  */
static unsigned pagesize = 0;  /* Shall be set later */

/* This data is used in sl_this call */
static pid_t        this_pid  = 0;
static const char*  this_name = "main";
static time_t       this_epoch= 0;

/* ========================================================================= *
 * Common Local methods (sl_XXX).
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * sl_is_main -- check to which process we are - controlling or testing
 * parameters: nothing
 * returns: nothing.
 * ------------------------------------------------------------------------- */
static inline int sl_is_main(void)
{
  return (NULL != opts.pids);
} /* sl_is_main */

/* ------------------------------------------------------------------------- *
 * sl_this -- return line which contains current process information for output.
 * parameters: nothing
 * returns: static string.
 * ------------------------------------------------------------------------- */
static const char* sl_this(void)
{
  static char buffer[64];

  snprintf(buffer, sizeof(buffer), "%5u %s [%u]:", (unsigned)(time(NULL) - this_epoch), this_name, this_pid);
  return buffer;
} /* sl_this */


/* ------------------------------------------------------------------------- *
 * sl_succ -- produces successor value according to mode and current value.
 * parameters: mode, current value
 * returns: new value.
 * ------------------------------------------------------------------------- */

static unsigned sl_succ(SL_MODE mode, unsigned current, unsigned limit)
{
  switch (mode)
  {
    case SL_Unknown:
    case SL_Linear:
        current++;
        break;

    case SL_Random:
        current = (unsigned)random();
        break;

    case SL_PseudoRandom:
        if (limit < SL_PSEUDO_RANDOM)
        {
          /* -2 .. 2 distribution */
          current += ((unsigned)random() % 5) - 2;
        }
        else
        {
          current += ((unsigned)random() % SL_PSEUDO_RANDOM) - SL_PSEUDO_RANDOM / 2 ;
        }
        /* check for crossing 0 is not necessary due to working with unsigned values ! */
        break;
  }

  return (current % limit);
} /* sl_succ */

/* ------------------------------------------------------------------------- *
 * sl_wait_info -- wait for signal arrived.
 * parameters: nothing
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void sl_wait_info(void)
{
  while (0 == info_flag)
    usleep(0);
} /* sl_wait_info */

/* ------------------------------------------------------------------------- *
 * sl_send_info -- send informational signal to pointed pid.
 * parameters: pid or 0 (for parent)
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void sl_send_info(pid_t pid)
{
  if ( pid )
    kill(pid, SL_SIGINFO);
  else
    kill(getppid(), SL_SIGINFO);
} /* sl_send_info */

/* ------------------------------------------------------------------------- *
 * sl_info_handler -- handler for information exchange, just turn info_flag on.
 * parameters: signal received
 * returns: nothing.
 * ------------------------------------------------------------------------- */
static void sl_info_handler(int signo)
{
  /* Make compiler happy */
  signo = signo;
  /* Notify main process */
  info_flag = 1;
} /* sl_info_handler */

/* ------------------------------------------------------------------------- *
 * sl_done_handler -- handler for termination signal, for main process shall
 *                    kill children first.
 * parameters: signal received
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void sl_done_handler(int signo)
{
  /* Make compiler happy */
  signo = signo;

  /* Kill all clients if we are main */
  if ( sl_is_main() )
  {
    unsigned index;

    printf ("%s killing %u clients\n", sl_this(), opts.clients);
    for (index = 0; index < opts.clients; index++)
      kill(opts.pids[index], SL_SIGDONE);
  }

  printf ("%s done\n", sl_this());

  /* In all cases we shall finish this application */
  exit(0);
} /* sl_done_handler */

/* ========================================================================= *
 * Test (client) Local methods (slc_XXX).
 * ========================================================================= */

static int* workset = NULL;   /* Data to be accessed at the client side */


/* ------------------------------------------------------------------------- *
 * slc_die -- kill all tests and controller.
 * parameters: nothing
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void slc_die(void)
{
  const pid_t father = getppid();
  printf ("%s killing test controller %u\n", sl_this(), father);
  kill(father, SL_SIGDONE);
} /* slc_die */

/* ------------------------------------------------------------------------- *
 * slc_init -- initialize required space of data.
 * parameters: nothing
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void slc_init(void)
{
  /* We are in client */
  this_name = "test";
  this_pid  = getpid();

  free(opts.pids);
  opts.pids = NULL;

  /* Initialize workset */
  workset = (int*) malloc(opts.workset * pagesize);
  if (NULL == workset)
  {
    printf ("[%s no space available to create test set\n", sl_this());
    slc_die();
  }
  else
  {
    memset(workset, 0x55, opts.workset * pagesize);
    printf ("%s initialization completed\n", sl_this());
    sl_send_info(0);
  }
} /* slc_init */

/* ------------------------------------------------------------------------- *
 * slc_main -- main testing function.
 * parameters: nothing
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void slc_main(void)
{
  const unsigned i2p_shift = 10;  /* shift left which equal to pagesize / sizeof(*workset) */

  info_flag = 0;
  while (1)
  {
    unsigned iterations;
    unsigned page_id = 0;
    int*     page_ptr;

    printf ("%s waiting for test run start\n", sl_this());
    sl_wait_info();
    info_flag = 0;
    printf ("%s test run started\n", sl_this());

    for (iterations = 0; iterations < opts.workset; iterations++)
    {
      /* access to page with number pageid */
      page_ptr = workset + (page_id << i2p_shift);
      *page_ptr ^= *page_ptr;

      /* calculate the next page */
      page_id = sl_succ(opts.pg_mode, page_id, opts.workset);
    }

    printf ("%s test run finished\n", sl_this());
    sl_send_info(0);
  } /* while testing loop */
} /* slc_main */

/* ========================================================================= *
 * Main (controller) Local methods (slm_XXX).
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * slm_getmode -- returns mode ID by letter.
 * parameters: letter with mode letter (L, P, R, etc)
 * returns: appropriate mode or SL_Unknown.
 * ------------------------------------------------------------------------- */

static SL_MODE slm_getmode(const char m)
{
  switch (m)
  {
    case 'L': return SL_Linear;
    case 'R': return SL_Random;
    case 'P': return SL_PseudoRandom;
  }

  return SL_Unknown;
} /* slm_getmode */

/* ------------------------------------------------------------------------- *
 * slm_getmodestr -- returns mode name by ID.
 * parameters: mode
 * returns: appropriate name of mode.
 * ------------------------------------------------------------------------- */

static const char* slm_getmodestr(SL_MODE mode)
{
  static const char* name[] = { "Unknown", "Linear", "Random", "Pseudo-Random" };
  return (0 < mode && mode < SL_CAPACITY(name) ? name[mode] : name[0]);
} /* slm_getmodestr */


/* ------------------------------------------------------------------------- *
 * slm_dump_params -- dumping information about parameters passed to application.
 * parameters: nothing.
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void slm_dump_params(void)
{
  const char* th = sl_this();

  printf ("%s number of clients set to %u\n", th, opts.clients);
  printf ("%s working set is %u MB (%u pages, %u bytes each)\n", th, (opts.workset * pagesize) >> 20, opts.workset, pagesize);
  printf ("%s test duration limit %u seconds\n", th, (unsigned)opts.t_limit);
  printf ("%s client selection mode is %s\n", th, slm_getmodestr(opts.cl_mode));
  printf ("%s pages selection mode is %s\n", th, slm_getmodestr(opts.pg_mode));
} /* slm_dump_params */


/* ------------------------------------------------------------------------- *
 * slm_main -- main part of test controller: select test, kick, wait for done.
 * parameters: nothing.
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void slm_main(void)
{
  unsigned current = 0;

  printf ("%s test cycle is started for %u seconds\n", sl_this(), (unsigned)opts.t_limit);
  alarm(opts.t_limit);
  while (1)
  {
    printf ("%s client %u with pid %u selected\n", sl_this(), current + 1, opts.pids[current]);
    info_flag = 0;
    sl_send_info(opts.pids[current]);
    sl_wait_info();

    /* Select the client to run actual test */
    current = sl_succ(opts.cl_mode, current, opts.clients);
  }
  printf ("%s test cycle is finished\n", sl_this());
} /* slm_main */


/* ------------------------------------------------------------------------- *
 * slm_usage -- show usage of application
 * parameters: application name
 * returns: nothing.
 * ------------------------------------------------------------------------- */

static void slm_usage(const char* self)
{
  printf ("\n");
  printf ("this application occupies required amount of memory and makes acceess\n");
  printf ("for reading and updating pages to generate load for virtual memory and swapping.\n");
  printf ("\n");
  printf ("%s can be invoked using the following mandatory parameters\n", self);
  printf ("in its command line:\n");
  printf ("- clients  - number of clients to be executed simultaneously\n");
  printf ("- size     - size of workset for each client, megabytes\n");
  printf ("- duration - test duration in seconds, 0 means no time limit\n");
  printf ("- type     - test type, could be combination for client activation\n");
  printf ("             and accessed pages pattern, main controls\n");
  printf ("     L - linear round robin access, one by one from 0 to X\n");
  printf ("     R - random access, somewhere in between 0..X\n");
  printf ("     P - pseudo-random, from current point +- %u percents of 0..X,\n", SL_PSEUDO_RANDOM);
  printf ("         if X is below %u than next will be -1, 0 or +1.\n", SL_PSEUDO_RANDOM);
  printf ("\n");
  printf ("examples:\n");
  printf ("  %s 8 128 120 LL - 8 clients, 128 MB per each, 120 seconds, lin/lin access\n", self);
  printf ("  %s 8 256 0 RP   - 8 clients, 256 MB per each, non-stop, random client selection, pseudo-random pages access\n", self);
} /* slm_usage */

/* ========================================================================= *
 * Main function.
 * ========================================================================= */

int main(const int argc, const char* argv[])
{
  unsigned index;
  pid_t    postfork;

  this_epoch = time(NULL);
  this_pid   = getpid();
  pagesize   = (unsigned)getpagesize();
  printf ("stress paging/swapping load generator, build %s %s\n", __DATE__, __TIME__);

  /* validate parameters in general */
  if (5 != argc)
  {
    slm_usage(argv[0]);
    return 1;
  }

  /* parse parameters one by one */
  memset(&opts, 0, sizeof(opts));
  opts.clients = atoi(argv[1]);
  opts.workset = atoi(argv[2]) * (1024 * 1024 / pagesize);
  opts.t_limit = (time_t)atoi(argv[3]);

  opts.cl_mode = slm_getmode(argv[4][0]);
  if (SL_Unknown == opts.cl_mode)
  {
    slm_usage(argv[0]);
    return 1;
  }

  opts.pg_mode = slm_getmode(argv[4][1]);
  if (SL_Unknown == opts.pg_mode)
  {
    slm_usage(argv[0]);
    return 1;
  }

  slm_dump_params();

  /* initialize all clients */
  signal(SL_SIGTIME, sl_done_handler);
  signal(SL_SIGDONE, sl_done_handler);
  signal(SL_SIGINFO, sl_info_handler);
  opts.pids = (pid_t*) calloc(opts.clients, sizeof(pid_t));

  for (index = 0; index < opts.clients; index++)
  {
    printf ("%s creating test client %u\n", sl_this(), index + 1);

    /* One more fork() usage example */
    info_flag = 0;
    postfork = fork();
    switch (postfork)
    {
      case 0:   /* I am a child */
          slc_init();
          slc_main();
          raise(SL_SIGDONE);
        break;

      case -1:  /* Bug happened */
          printf ("%s error %d - %s\n", sl_this(), errno, strerror(errno));
          raise(SL_SIGDONE);
        break;

      default:  /* I am a parent */
          opts.pids[index] = postfork;
          printf ("%s waiting for test client %u pid %u initialization\n", sl_this(), index + 1, postfork);
          sl_wait_info();
        break;
    }
  } /* for */

  /* run the test */
  slm_main();

  /* finalize all clients */
  raise(SL_SIGDONE);

  return 0;
} /* main */

/* ========================================================================= *
 *                    No more code in file swpload.c                         *
 * ========================================================================= */
