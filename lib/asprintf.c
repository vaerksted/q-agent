/* aprintf.c - asprintf functions for systems missing it
 * Copyright (C) 1999 Robert Bihlmeyer <robbe@orcus.priv.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "asprintf.h"

#ifdef HAVE_VSNPRINTF
int asprintf(char **result, const char *fmt, ...)
{
  va_list args;
  int ret;

  va_start(args, fmt);
  ret = vasprintf(result, fmt, args);
  va_end(args);
  return ret;
}

int vasprintf(char **result, const char *fmt, va_list args)
{
#define INITIAL_SIZE 100
  char *s;
  int len;

  s = (char *)malloc(INITIAL_SIZE);
  if (!s) {
    *result = NULL;
    return -1;
  }
  len = vsnprintf(s, INITIAL_SIZE, fmt, args);
  if (len >= INITIAL_SIZE) {
    s = (char *)realloc(s, len+1);
    if (!s) {
      *result = NULL;
      return -1;
    }
    len = vsnprintf(s, len+1, fmt, args);
  }
  if (len >= 0)
    *result = s;
  else
    *result = NULL;
  return len;
}
#else
#include "vasprintf.c"
#endif
