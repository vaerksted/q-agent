/* Quintuple Agent test client
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
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/mman.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else 
#include "getopt.h"
#endif /* HAVE_GETOPT_H */

#include "i18n.h"
#include "agent.h"
#include "agentlib.h"
#include "memory.h"
#include "util.h"

int debug = 0;
char *query_options = "";

/* usage - show usage */
void usage()
{
    printf(_("Usage: q-client [OPTION]... put ID [COMMENT]\n\
       q-client [OPTION]... {get|delete} ID\n\
       q-client [OPTION]... list\n\
`put' reads a secret from stdin and stores it with the agent under ID with\n\
COMMENT, if specified, attached to it.\n\
`get' fetches the secret under ID, and prints it to stdout.\n\
`delete' induces the agent to forget the secret under ID.\n\
`list' lists the ids of all known secrets along with their comments.\n\
\n\
Options relevant to `put':\n\
  -i, --insure             ask again, before giving out a secret\n\
  -q, --query-options OPT  pass options OPT through to the query program\n\
  -t, --time-to-live N     forget the secret after N seconds\n\
\n\
General options:\n\
  -d, --debug            turn on debugging output\n\
      --help             display this help and exit\n\
      --version          output version information and exit\n\n"));
}

/* check_status - if debugging, output an interpretation of the status code */
void check_status(status_t re)
{
  char *status;

  if (!debug)
    return;
  switch (re) {
  case STATUS_OK: status = "OK"; break;
  case STATUS_FAIL: status = "FAIL"; break;
  case STATUS_COMM_ERR: status = "COMM_ERR"; break;
  default: assert(0);
  }
  debugmsg("agent replied: %s\n", status);
}

/* xgetpass - get a passphrase into a "secure" static storage
   Output PROMPT and then input the password with echo turned off.
   The password is stored in a buffer that is newly allocated in secure
   storage. secmem_free should be called when the buffer is no longer needed.
   There are no guarantees that the stdio library does not have copies of the
   passphrase lying around. Sorry. */
char *xgetpass(char *prompt)
{
  char *buf, *bp;
  size_t len, bufsize = 1000;
  struct termios old, new;
  FILE *pstream = NULL;

  if (isatty(STDIN_FILENO)) {
    if (tcgetattr(STDIN_FILENO, &old) != 0) {
      perror(_("could not get terminal characteristics"));
      return NULL;
    }
    new = old;
    new.c_lflag &= ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new) != 0) {
      perror(_("could not turn echoing off"));
      return NULL;
    }
  }
#ifdef HAVE_GTK
  else if (getenv("DISPLAY")) {
    char *buf;
    if (asprintf(&buf, "%s %s '%s'", QUERY_PROGRAM, query_options, prompt)
	< 0) {
      perror(_("out of memory"));
      return NULL;
    }
    if (!(pstream = popen(buf, "r")))
      fprintf(stderr, _("could not fork off %s: %s\n"), QUERY_PROGRAM,
	      strerror(errno));
    free(buf);
  }
#endif
  buf = (char *)secmem_malloc(bufsize);
  if (!buf) {
    fprintf(stderr, _("could not allocate space in secure storage\n"));
    return NULL;
  }
  if (!pstream)
    printf("%s", prompt);
  len = 0;
  while (1) {
    bp = buf + len;
    if (!fgets(bp, bufsize - len, pstream ? pstream : stdin)) {
      perror(_("error while reading"));
      secmem_free(buf);
      return NULL;
    }
    len += strlen(bp);
    if (buf[len-1] == '\n')
      break;			/* we've read the entire line */
    bufsize *= 2;
    buf = (char *)secmem_realloc(buf, bufsize);
    if (!buf) {
      fprintf(stderr, _("could not allocate space in secure storage\n"));
      return NULL;
    }
  }
  bp[len-1] = 0;
  if (pstream)
    pclose(pstream);
  if (isatty(STDIN_FILENO) && tcsetattr(STDIN_FILENO, TCSAFLUSH, &old) != 0)
    perror(_("could not turn echoing back on"));
  putchar('\n');
  return buf;
}  

/* main - read commands & arguments, execute them */
int main(int argc, char **argv)
{
  int opt, opt_insure = 0, opt_help = 0, opt_version = 0;
  char *opt_ttl = NULL;
  struct option opts[] = {{ "debug",	     no_argument,	 NULL,  'd' },
			  { "insure",	     no_argument,	 NULL,	'i' },
			  { "query-options", required_argument,  NULL,  'q' },
			  { "time-to-live",  required_argument,  NULL,  't' },
			  { "help",	     no_argument,  &opt_help,	 1  },
			  { "version",	     no_argument,  &opt_version, 1  },
			  { NULL, 0, NULL, 0 } };
  enum { CMD_List, CMD_Put, CMD_Get, CMD_Delete } command;
  char *Commands[] = { "list", "put", "get", "delete" };
  status_t status;

  secmem_init(1);		/* 1 is too small, so default size is used */
  secmem_set_flags(SECMEM_WARN);
  drop_privs();

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  while ((opt = getopt_long(argc, argv, "diq:t:", opts, NULL)) != -1)
    switch (opt) {
    case 'd':
      debug = 1;
      break;
    case 'i':
      opt_insure = 1;
      break;
    case 't':
      opt_ttl = optarg;
      break;
    case 'q':
      query_options = optarg;
      break;
    case 0:
    case '?':
      break;
    default:
      assert(0);
    }
  if (opt_version) {
    printf("q-client " VERSION " (" PACKAGE ")\n");
    exit(EXIT_SUCCESS);
  }
  if (opt_help) {
    usage();
    exit(EXIT_SUCCESS);
  }
  if (optind >= argc) {
    fprintf(stderr, _("you must specify a command\n"));
    exit(EXIT_FAILURE);
  }
  for (command = 0; command < sizeof(Commands)/sizeof(Commands[0]); ++command)
    if (strcmp(argv[optind], Commands[command]) == 0)
      break;
  if (command >= sizeof(Commands)/sizeof(Commands[0])) {
    fprintf(stderr, _("command must be one of: put, get, delete, list\n"));
    usage();
    exit(EXIT_FAILURE);
  }
  if (agent_init() < 0)
    exit(EXIT_FAILURE);
  if (command != CMD_Put) {
    if (opt_insure)
      fprintf(stderr,
	      _("%s option has no meaning with %s command - ignored\n"),
	      "insure", Commands[command]);
    if (opt_ttl)
      fprintf(stderr,
	      _("%s option has no meaning with %s command - ignored\n"),
	      "time-to-live", Commands[command]);
  }
  if (command == CMD_List) {
    reply_list *reply;
    if (optind != argc-1) {
      fprintf(stderr, _("list wants no arguments\n"));
      exit(EXIT_FAILURE);
    }
    if (!(reply = malloc(sizeof(reply_list)))) {
      fprintf(stderr, _("out of memory\n"));
      exit(EXIT_FAILURE);
    }
    status = agent_list(&reply);
    check_status(status);
    if (status == STATUS_OK) {
      unsigned i;
      for (i = 0; i < reply->entries; i++) {
	char dl[20];
	if (reply->entry[i].deadline)
	  strftime(dl, 20, "%Y-%m-%d %H:%M:%S",
		   localtime(&reply->entry[i].deadline));
	else
	  dl[0] = 0;
	printf("%s\t%-20s\t%s\t%s\n", reply->entry[i].id,
	       dl[0] ? dl : _("none"),
	       (reply->entry[i].flags & FLAGS_INSURE) ? "insure" : "",
	       reply->entry[i].comment);
      }
    }
    free(reply);
  } else if (command == CMD_Put) {
    char *s, *c, *buf;
    flags_t flags = 0;
    time_t deadline;
    if (optind+1 == argc-1 ) {
      c = "";
    } else if (optind+2 == argc-1) {
      c = argv[optind+2];
    } else {
      fprintf(stderr, _("put wants one or two arguments\n"));
      usage();
      exit(EXIT_FAILURE);
    }
    if (asprintf(&buf, _("Enter secret to store under \"%s\": "),
		 argv[optind+1]) < 0) {
      perror(_("out of memory"));
      exit(EXIT_FAILURE);
    }
    s = xgetpass(buf);
    free(buf);
    if (!s)
      exit(EXIT_FAILURE);
    flags = 0;
    if (opt_insure)
      flags |= FLAGS_INSURE;
    if (opt_ttl) {
      char *err;
      deadline = time(NULL);
      deadline += strtoul(opt_ttl, &err, 10);
      if (*err) {
	fprintf(stderr, _("%s: invalid time-to-live\n"), opt_ttl);
	exit(EXIT_FAILURE);
      }
    } else {
      deadline = 0;
    }
    status = agent_put(argv[optind+1], flags, deadline, c, s);
    secmem_free(s);
    check_status(status);
  } else if (command == CMD_Get) {
    reply_get *reply;
    if (optind+1 != argc-1) {
      fprintf(stderr, _("get wants exactly one argument\n"));
      usage();
      exit(EXIT_FAILURE);
    }
    status = agent_get(argv[optind+1], &reply);
    check_status(status);
    if (status == STATUS_OK) {
      if (isatty(STDOUT_FILENO))
	printf(_("secret available, but I won't print it on a tty\n"));
      else
	puts(reply->data);
    }
  } else if (command == CMD_Delete) {
    if (optind+1 != argc-1) {
      fprintf(stderr, _("delete wants exactly one argument\n"));
      usage();
      exit(EXIT_FAILURE);
    }
    status = agent_delete(argv[optind+1]);
    check_status(status);
  } else
    assert(0);
  agent_done();
  secmem_term();
  exit(status == STATUS_OK ? EXIT_SUCCESS : status == STATUS_FAIL ? 2 : 3);
}
