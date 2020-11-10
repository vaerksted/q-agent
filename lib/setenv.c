/* setenv.c - setenv & unsetenv functions for systems missing it
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "setenv.h"

#ifdef TESTING
#define setenv my_setenv
#define unsetenv my_unsetenv
#define TEST(F,E) do { printf("%s = %" #F "\n", #E, E); } while(0)
#define TEST0(E)   do { printf("%s\n", #E); E; } while (0)
#endif

/* some systems do not declare this in their headers */
extern char **environ;

int setenv(const char *name, const char *value, int replace)
{
  char *s;
  size_t namelen;

  if (!replace && getenv(name))
    return 0;
  namelen = strlen(name);
  s = malloc(namelen + 1 + strlen(value) + 1);
  if (!s)
    return -1;
  strcpy(s, name);
  s[namelen] = '=';
  strcpy(s + namelen + 1, value);
  putenv(s);
  return 0;
}

void unsetenv(const char *name)
{
  char **v;
  size_t len;

  len = strlen(name);
  for (v = environ; v[0] != NULL; v++)
    if (strncmp(v[0], name, len) == 0 && v[0][len] == '=')
      break;
  while (v[0]) {
    v[0] = v[1];
    v++;
  }
}

#ifdef TESTING
#include <stdio.h>

int main()
{
  TEST(d,setenv("xxx", "foo", 1));
  TEST(s,getenv("xxx"));
  TEST(d,setenv("xxx", "bar", 0));
  TEST(s,getenv("xxx"));
  TEST(d,setenv("xxx", "baz", 1));
  TEST(s,getenv("xxx"));
  TEST0(unsetenv("xxx"));
  TEST(s,getenv("xxx"));
  return 0;
}
#endif
