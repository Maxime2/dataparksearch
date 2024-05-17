/* Copyright (C) 2003-2006 Datapark corp. All rights reserved.
   Based on Electric Fence 2.2 by Bruce Perens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "dps_common.h"

#ifdef EFENCE

#include "dps_efence.h"
#include "dps_charsetutils.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

/*
 * These routines do their printing without using stdio. Stdio can't
 * be used because it calls malloc(). Internal routines of a malloc()
 * debugger should not re-enter malloc(), so stdio is out.
 */

/*
 * NUMBER_BUFFER_SIZE is the longest character string that could be needed
 * to represent an unsigned integer, assuming we might print in base 2.
 */
#define NUMBER_BUFFER_SIZE (sizeof (ef_number) * NBBY)

static void
do_abort (void)
{
  /*
   * I use kill(getpid(), SIGILL) instead of abort() because some
   * mis-guided implementations of abort() flush stdio, which can
   * cause malloc() or free() to be called.
   */
  kill (getpid (), SIGILL);
  /* Just in case something handles SIGILL and returns, exit here. */
  _exit (-1);
}

static void
printNumber (ef_number number, ef_number base)
{
  char buffer[NUMBER_BUFFER_SIZE];
  char *s = &buffer[NUMBER_BUFFER_SIZE];
  int size;

  do
    {
      ef_number digit;

      if (--s == buffer)
        EF_Abort ("Internal error printing number.");

      digit = number % base;

      if (digit < 10)
        *s = '0' + digit;
      else
        *s = 'a' + digit - 10;
    }
  while ((number /= base) > 0);

  size = &buffer[NUMBER_BUFFER_SIZE] - s;

  if (size > 0)
    write (2, s, size);
}

void
EF_Printv (const char *pattern, va_list args)
{
  static const char bad_pattern[] =
      "\nBad pattern specifier %%%c in EF_Print().\n";
  const char *s = pattern;
  char c;

  while ((c = *s++) != '\0')
    {
      if (c == '%')
        {
          c = *s++;
          switch (c)
            {
            case '%':
              (void) write (2, &c, 1);
              break;
            case 'a':
              /*
               * Print an address passed as a void pointer.
               * The type of ef_number must be set so that
               * it is large enough to contain all of the
               * bits of a void pointer.
               */
              printNumber (
                  (ef_number) va_arg (args, void *), 0x10);
              break;
            case 's':
              {
                const char *string;
                size_t length;

                string = va_arg (args, char *);
                length = dps_strlen (string);

                (void) write (2, string, length);
              }
              break;
            case 'd':
              {
                int n = va_arg (args, int);

                if (n < 0)
                  {
                    char cc = '-';
                    write (2, &cc, 1);
                    n = -n;
                  }
                printNumber (n, 10);
              }
              break;
            case 'x':
              printNumber (va_arg (args, u_int), 0x10);
              break;
            case 'c':
              {
                char c = va_arg (args, int); /* Note: char is promoted to int. */

                (void) write (2, &c, 1);
              }
              break;
            default:
              {
                EF_Print (bad_pattern, c);
              }
            }
        }
      else
        (void) write (2, &c, 1);
    }
}

void
EF_Abortv (const char *pattern, va_list args)
{
  EF_Print ("\nMemDebug Aborting: ");
  EF_Printv (pattern, args);
  EF_Print ("\n");
  do_abort ();
}

void
EF_Abort (const char *pattern, ...)
{
  va_list args;

  va_start (args, pattern);
  EF_Abortv (pattern, args);
  /* Not reached: va_end(args); */
}

void
EF_Exitv (const char *pattern, va_list args)
{
  EF_Print ("\nMemDebug Exiting: ");
  EF_Printv (pattern, args);
  EF_Print ("\n");

  /*
   * I use _exit() because the regular exit() flushes stdio,
   * which may cause malloc() or free() to be called.
   */
  _exit (-1);
}

void
EF_Exit (const char *pattern, ...)
{
  va_list args;

  va_start (args, pattern);

  EF_Exitv (pattern, args);

  /* Not reached: va_end(args); */
}

void
EF_Print (const char *pattern, ...)
{
  va_list args;

  va_start (args, pattern);
  EF_Printv (pattern, args);
  va_end (args);
}

void
EF_InternalError (const char *pattern, ...)
{
  va_list args;

  EF_Print ("\nInternal error in allocator: ");
  va_start (args, pattern);
  EF_Printv (pattern, args);
  EF_Print ("\n");
  va_end (args);
  do_abort ();
}

#endif /* EFENCE */
