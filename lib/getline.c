/* getline.c - getdelim & getline functions for systems missing it
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

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "getline.h"

#define INITIAL_BUFFER_SIZE 120

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
  return getdelim(lineptr, n, '\n', stream);
}

#ifndef HAVE_GETDELIM
ssize_t getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream)
{
  ssize_t len;
  int c;

  if (!*lineptr || !*n) {
    *n = INITIAL_BUFFER_SIZE;
    if (!(*lineptr = (char *)malloc(*n)))
      return -1;
  }

  for (len = 0; (c = getc(stream)) != EOF; ) {
    (*lineptr)[len++] = c;
    if (c == delimiter)
      break;
    if (len == *n - 1) {
      *n *= 2;
      if (!(*lineptr = realloc(*lineptr, *n))) {
	return -1;
      }
    }
  }
  (*lineptr)[len] = 0;
  return len;
}
#endif /* HAVE_GETDELIM */
