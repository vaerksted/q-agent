/* Quintuple Agent pgp2.6 wrapper
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

#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else 
#include "getopt.h"
#endif /* HAVE_GETOPT_H */

#include "i18n.h"
#include "agentlib.h"
#include "memory.h"
#include "util.h"

#ifndef HAVE_STRDUP
#include "strdup.h"
#endif

#define PGP	"pgp"
#define PGP_KEYLIST	PGP \
	" +pubring=$HOME/.pgp/secring.pgp -kv '%s' 2>/dev/null"

int debug = 0;

/* find_id - scans ARGV for those arguments that affect which secret key is
   used, then forks off pgp to list possible secret keys, and returns the
   first one.
   The returned string should be free()'d after use. */ 
char *find_id(int argc, char **argv)
{
  int opt_version = 0;
  struct option opts[] = {
    { "version",	no_argument,		&opt_version,	1  },
    { NULL, 0, NULL, 0 }
  };
  int opt;
  FILE *pgp;
  char *id = NULL, *buf, *line = NULL;
  size_t size;
  ssize_t len;

  opterr = 0;
  while ((opt = getopt_long(argc, argv, "+@abcdefk:mo:pstu:wz", opts, NULL))
	 != -1) {
    if (opt == 'u') {
      id = optarg;
      break;
    }
  }
  if (opt_version) {
    printf("apgp " VERSION " (" PACKAGE ")\n\n");
    execlp(PGP, PGP, NULL);
    fprintf(stderr, _("could not exec %s: %s"), PGP, strerror(errno));
    exit(EXIT_FAILURE);
  }
  for (; optind < argc; optind++)
    if (!strncasecmp(argv[optind], "+myname=", 8))
      id = argv[optind] + 8;

  if (asprintf(&buf, PGP_KEYLIST, id ? id : "") < 0) {
    fprintf(stderr, _("out of memory\n"));
    return NULL;
  }
  if (!(pgp = popen(buf, "r"))) {
    fprintf(stderr,
	    _("could not fork off %s: %s\n"), buf, strerror(errno));
    free(buf);
    return NULL;
  }
  free(buf);
  while ((len = getline(&line, &size, pgp)) > 0) {
    if (len > 9 && !strncmp(line, "sec ", 4) && line[9] == '/') {
      char *x;
      if ((x = strchr(line + 10, ' ')) != NULL) {
	*x = 0;
	id = strdup(line + 10);
	free(line);
	pclose(pgp);
	return id;
      }
    }
  }
  pclose(pgp);
  fprintf(stderr, _("could not determine key id\n"));
  return NULL;
}

int main(int argc, char **argv)
{
  pid_t child;
  int p[2];
  char *id;

  secmem_init(1);
  secmem_set_flags(SECMEM_WARN);
  drop_privs();

  if (!(id = find_id(argc, argv))) {
    return EXIT_FAILURE;
  }
  if (pipe(p) < 0) {
    perror(_("could not create pipe"));
    return EXIT_FAILURE;
  }
  if ((child = fork()) < 0) {
    perror(_("could not fork"));
    return EXIT_FAILURE;
  } else if (child > 0) {
    reply_get *r;
    int status;

    close(p[0]);
    if (agent_init() >= 0) {
      if (agent_get(id, &r) == STATUS_OK) {
	if (write(p[1], r->data, strlen(r->data)) < 0)
	  perror(_("error while writing passphrase"));
      } else {
	fprintf(stderr, _("agent could not provide passphrase\n"));
      }
    }
    if (close(p[1]) < 0)
      perror(_("could not finish writing passphrase"));
    agent_done();
    if (waitpid(child, &status, 0) < 0) {
      perror(_("could not wait for child"));
      return EXIT_FAILURE;
    }
    if (WIFEXITED(status))
      return WEXITSTATUS(status);
    else
      return EXIT_FAILURE;
  } else {
    char **args, *buf;
    int i;

    close(p[1]);
    args = malloc(sizeof(char *) * (argc + 2));
    if (!args) {
      fprintf(stderr, _("out of memory\n"));
      return EXIT_FAILURE;
    }
    if (asprintf(&buf, "%d", p[0]) < 0) {
      fprintf(stderr, _("out of memory\n"));
      return EXIT_FAILURE;
    }
    setenv("PGPPASSFD", buf, 1);
    free(buf);
    args[0] = PGP;
    for (i = 1; i < argc; i++)
      args[i] = argv[i];
    args[i] = NULL;
    execvp(PGP, args);
    fprintf(stderr, _("could not exec %s: %s"), PGP, strerror(errno));
    return EXIT_FAILURE;
  }
}
