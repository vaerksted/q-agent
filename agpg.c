/* Quintuple Agent gpg wrapper
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

#define GPG		"gpg"
#define GPG_KEYLIST	GPG " --list-secret-keys"
#define GPG_KEYLISTID	GPG " --list-secret-keys '%s'"

int debug = 0;

/* find_id - scans ARGV for those arguments that affect which secret key is
   used, then forks off gpg to list possible secret keys, and returns the
   first one.
   The returned string should be free()'d after use. */ 
char *find_id(int argc, char **argv)
{
  int opt_version = 0;
  struct option opts[] = {
    { "local-user",	required_argument,	NULL,		'u'},
    { "lock-once",	no_argument,		NULL,		'?'},
    { "version",	no_argument,		&opt_version,	1  },
    { NULL, 0, NULL, 0 }
  };
  int opt;
  FILE *gpg;
  char *id = NULL, *buf, *line = NULL;
  size_t size;
  ssize_t len;

  opterr = 0;
  while ((opt = getopt_long(argc, argv, "+abcdekno:qr:stu:vz", opts, NULL))
	 != -1) {
    if (opt == 'u') {
      id = optarg;
      break;
    }
  }
  if (opt_version) {
    printf("agpg " VERSION " (" PACKAGE ")\n");
    execlp(GPG, GPG, "--version", NULL);
    fprintf(stderr, _("could not exec %s: %s\n"), GPG, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (id) {
    if (asprintf(&buf, GPG_KEYLISTID, id) < 0) {
      fprintf(stderr, _("out of memory\n"));
      return NULL;
    }
  } else
    buf = GPG_KEYLIST;
  if (!(gpg = popen(buf, "r"))) {
    fprintf(stderr, _("could not exec %s: %s\n"), buf, strerror(errno));
    if (id)
      free(buf);
    return NULL;
  }
  if (id)
    free(buf);
  while ((len = getline(&line, &size, gpg)) > 0) {
    if (len > 10 && !strncmp(line, "sec ", 4) && line[10] == '/') {
      char *x;
      if ((x = strchr(line + 11, ' ')) != NULL) {
	*x = 0;
	id = strdup(line + 11);
	free(line);
	pclose(gpg);
	return id;
      }
    }
  }
  pclose(gpg);
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

  if (getenv("AGPG_RUNNING")) {
    fprintf(stderr, _("agpg running inside itself, do you perhaps have gpg point to agpg?\n"
	      "Bailing out ...\n"));
    return EXIT_FAILURE;
  }
  if (setenv("AGPG_RUNNING", "1", 0) < 0) {
    perror(_("could not set $AGPG_RUNNING"));
    return EXIT_FAILURE;
  }

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
    char **args;
    int i;

    close(p[1]);
    args = malloc(sizeof(char *) * (argc + 2));
    if (!args) {
      fprintf(stderr, _("out of memory\n"));
      return EXIT_FAILURE;
    }
    args[0] = GPG;
    args[1] = "--passphrase-fd";
    if (asprintf(&args[2], "%d", p[0]) < 0) {
      fprintf(stderr, _("out of memory\n"));
      return EXIT_FAILURE;
    }
    for (i = 1; i < argc; i++)
      args[i+2] = argv[i];
    args[i+2] = NULL;
    execvp(GPG, args);
    return EXIT_FAILURE;
  }
}
