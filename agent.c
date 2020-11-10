/* Quintuple Agent
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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <glib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else 
#include "getopt.h"
#endif /* HAVE_GETOPT_H */

#include "asprintf.h"
#include "i18n.h"
#include "memory.h"
#include "agent.h"
#include "util.h"

#ifndef HAVE_STRDUP
#include "strdup.h"
#endif

/* commands to set an environment variable in the different shells */
#define SETENV_SH	"AGENT_SOCKET='%s'; export AGENT_SOCKET\n"
#define SETENV_CSH	"setenv AGENT_SOCKET '%s'\n"

#define TMP_DIR_TRIES	1000

struct value {
  char comment[COMMENT_LENGTH];
  char *data;
};

GHashTable *cache;
char *sockdir = NULL, *sockname = NULL;
int sock = -1;
int keep_going = 1;
int debug = 0;
char *query_options = "";
reply failed_reply = { REPLY_MAGIC, STATUS_FAIL };
time_t next_deadline = 0;
flags_t supported;
int x_enabled;


#define BLIND(x) ((debug >= 2) ? (x) : "XXX")

void exit_gracefully(int sig)
{
  keep_going = 0;
}

/* copy file descriptor old into a new slot, with error handling. */
static void
xdup2(int old, int new)
{
  if (dup2(old, new) < 0) {
    perror(_("could not duplicate file descriptor"));
    exit(EXIT_FAILURE);
  }
}

/* move an old file descriptor into a new slot.
   the descriptors formerly in new and old are closed afterwards.
*/
static void
move_fd(int old, int new)
{
  if (old != new) {
    xdup2(old, new);
    if (close(old) < 0) {
      perror(_("error closing temporary file descriptor"));
    }
  }
}

/* make a new temporary directory */
static int make_tmpdir()
{
  int i;
  uid_t uid;
  char *tmp;

  uid = getuid();
  if ((tmp = getenv("TMPDIR")) == NULL && (tmp = P_tmpdir) == NULL)
    tmp = "/tmp";
  for (i=0; i<TMP_DIR_TRIES; i++) {
    if (asprintf(&sockdir, "%s/%s-%d.%d", tmp, PACKAGE, uid, i) < 0) {
      perror(_("could not assemble socket path"));
      return -1;
    }
    if (mkdir(sockdir, 0700) < 0) {
      if (errno != EEXIST) {
	perror(_("could not create socket directory"));
	return -1;
      }
    } else
      break;
    /* TODO: try to contact the other agent, and see if it is usable */
    free(sockdir);
  }
  if (i >= TMP_DIR_TRIES) {
    fprintf(stderr, _("giving up after %d tries\n"), TMP_DIR_TRIES);
    return -1;
  }
  return 0;
}

/* initializes the communication socket and binds it to a file path */
static int create_socket()
{
  size_t len, l;
  struct sockaddr_un *addr;

  if (make_tmpdir() < 0)
    return -1;
  if (!(sock = socket(PF_UNIX, SOCK_STREAM, 0))) {
    perror(_("could not create socket"));
    return -1;
  }
  l = strlen(sockdir);
  len = l + 1 + sizeof(SOCKET_NAME) + 1;
  if (!(sockname = malloc(len))) {
    perror(_("could not assemble socket name"));
    return -1;
  }
  strcpy(sockname, sockdir);
  sockname[l] = '/';
  strcpy(sockname+l+1, SOCKET_NAME);
  len += offsetof(struct sockaddr_un, sun_path);
  addr = alloca(len);
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, sockname);
  if (bind(sock, (struct sockaddr *)addr, len) < 0) {
    perror(_("could not bind socket"));
    return -1;
  }
  if (listen(sock, 5) < 0) {
    perror(_("could not listen to socket"));
    return -1;
  }
  return 0;
}

/* close all connections, and remove the server socket */
static void cleanup()
{
  if (sock >= 0 && close(sock) < 0)
    perror(_("error while closing socket"));
  if (sockname && unlink(sockname) < 0)
    perror(_("could not unlink socket"));
  if (sockdir && rmdir(sockdir) < 0)
    perror(_("could not remove socket directory"));
  if (debug)
    secmem_dump_stats();
  secmem_term();
}

/* remove a secret from the hash table, and free it */
void delete_secret(char *id)
{
  gpointer key, val;

  if (g_hash_table_lookup_extended(cache, id, &key, &val)) {
    g_hash_table_remove(cache, id);
    free(key);
    secmem_free(val);
  }
}

reply_get *store(char *id, flags_t flags, time_t deadline, char *comment,
		 char *data)
{
  reply_get *value;

  value = secmem_malloc(sizeof(reply_get));
  if (value) {
    debugmsg("storing at %p\n", value);
    value->magic = REPLY_MAGIC;
    value->status = STATUS_OK;
    value->flags = flags;
    value->deadline = deadline;
    strcpy(value->comment, comment);
    strcpy(value->data, data);
    /* delete old version cleanly, since it will be overwritten anyway */
    delete_secret(id);
    g_hash_table_insert(cache, strdup(id), value);
    if (deadline && (!next_deadline || deadline < next_deadline))
      next_deadline = deadline;
    return value;
  } else {
    fprintf(stderr, _("could not allocate space in secure storage\n"));
    return NULL;
  }
}

/* store a secret in secure memory */
void do_put(int client, request_put *req)
{
  reply rep;

  debugmsg("PUT %s, %lx, %ld, %s, %s\n", req->id, (long)req->flags,
	   (long)req->deadline, req->comment, BLIND(req->data));
  rep.magic = REPLY_MAGIC;
  if (req->flags & ~supported)
    rep.status = STATUS_FAIL;
  else 
    rep.status = store(req->id, req->flags, req->deadline, req->comment,
		       req->data) != NULL ? STATUS_OK : STATUS_FAIL;
  if (xwrite(client, &rep, sizeof(rep)) < 0)
    perror(_("error while replying"));
}

/* fetch a secret by id */
void do_get(int client, request_get *req)
{
  reply *rep;
  size_t size;
  int do_insurance = 1;

  debugmsg("GET %s\n", req->id);
  rep = (reply *)g_hash_table_lookup(cache, req->id);
  if (!rep) {
    if (x_enabled) {
      char *buf;
      if (asprintf(&buf,
		   _("%s %s -e 'Enter secret to store under \"%s\":'"),
		   QUERY_PROGRAM, query_options,
		   req->id) >= 0) {
	FILE *f;
	char *data;
	debugmsg("try calling '%s'\n", buf);
	if ((f = popen(buf, "r")) != NULL) {
	  int deadline = 0, flags = 0;
	  char buf[200];
	  while (1) {
	    char *d;
	    if (fgets(buf, 200, f) == NULL)
	      break;
	    buf[strlen(buf) - 1] = 0;
	    if (buf[0] == 0)
	      break;
	    if ((d = strstr(buf, ": ")) == NULL)
	      continue;
	    *d = 0;
	    d += 2;
	    debugmsg("keyword %s value %s\n", buf, d);
	    if (strcmp(buf, "Options") == 0) {
	      if (strcmp(d, "insure") == 0)
		flags |= FLAGS_INSURE;
	    } else if (strcmp(buf, "Timeout") == 0) {
	      deadline = strtoul(d, NULL, 10);
	      if (deadline)
		deadline += time(NULL);
	    }
	  }
	  if ((data = (char *)secmem_malloc(DATA_LENGTH)) != NULL) {
	    size_t len;
	    if (fgets(data, DATA_LENGTH, f) != NULL
		&& (len = strlen(data)) > 0) {
	      if (data[len-1] == '\n')
		data[len-1] = 0;
	      rep = (reply *)store(req->id, flags, deadline, "", data);
	      do_insurance = 0;
	    }
	    secmem_free(data);
	  } else {
	    fprintf(stderr, _("could not allocate space in secure storage\n"));
	    exit(EXIT_FAILURE);
	  }
	  pclose(f);
	}
	free(buf);
      }
    }
  }
  if (rep && ((reply_get *)rep)->flags & FLAGS_INSURE && do_insurance) {
    int pid;
    if ((pid = fork()) == 0) {
      char *buf;
      size_t len;
      len = strlen(((reply_get *)rep)->comment);
      asprintf(&buf, _("Hand out secret %s%s%s%s?"),
	       req->id, len ? " (" : "",
	       ((reply_get *)rep)->comment, len ? ")" : "");
#ifdef HAVE_GTK
      execlp("secret-ask", "secret-ask", "bool", buf, NULL);
#endif
#ifdef XMESSAGE
      execl(XMESSAGE, "xmessage", "-nearmouse", "-default", "yes",
	    "-buttons", "yes:2,no:3", buf, NULL);
#endif
      free(buf);
      perror(_("could not exec insure command"));
      exit(EXIT_FAILURE);
    } else if (pid < 0) {
      perror(_("could not fork"));
      rep = NULL;
    } else {
      int status;
      waitpid(pid, &status, 0);
      if (!WIFEXITED(status) || WEXITSTATUS(status) == 1) {
	fprintf(stderr, _("call of insure command failed\n"));
	supported &= ~FLAGS_INSURE;
	rep = NULL;
      } else if (WEXITSTATUS(status) == 3)
	rep = NULL;
    }
  }
  if (rep) {
    size = sizeof(reply_get);
    debugmsg("reply with %d bytes (%p): %s, %lx, %ld, %s, %s\n", size, rep,
	     ((reply_get *)rep)->status==STATUS_OK ? "OK" : "FAIL",
	     (long)((reply_get *)rep)->flags,
	     (long)((reply_get *)rep)->deadline,
	     ((reply_get *)rep)->comment,
	     BLIND(((reply_get *)rep)->data));
  } else {
    rep = &failed_reply;
    size = sizeof(failed_reply);
    debugmsg("reply with %d bytes: %s\n", size,
	     rep->status==STATUS_OK ? "OK" :"FAIL");
  }
  if (xwrite(client, rep, size) < 0)
    perror(_("error while replying"));
}

/* remove a secret by id */
void do_delete(int client, request_get *req)
{
  reply rep;

  debugmsg("DELETE %s\n", req->id);
  delete_secret(req->id);
  rep.magic = REPLY_MAGIC;
  rep.status = STATUS_OK;
  if (xwrite(client, &rep, sizeof(rep)) < 0)
    perror(_("error while replying"));
}

void send_list_entry(char *key, reply_get *value, int *fd)
{
  static reply_list_entry rep;

  if (*fd == -1)
    return;
  strncpy(rep.id, key, ID_LENGTH);
  rep.flags = value->flags;
  rep.deadline = value->deadline;
  strncpy(rep.comment, value->comment, COMMENT_LENGTH);
  debugmsg("sending entry %s\n", rep.id);
  if (xwrite(*fd, &rep, sizeof(rep)) < 0) {
    *fd = -1;
    perror(_("error while replying"));
  }
}

/* list ids and comments of all known secrets */
void do_list(int client)
{
  reply_list rep;
  int clnt;

  debugmsg("LIST\n");
  rep.magic = REPLY_MAGIC;
  rep.status = STATUS_OK;
  rep.entries = g_hash_table_size(cache);
  if (xwrite(client, &rep, sizeof(rep)) < 0) {
    perror(_("error while replying"));
    return;
  }
  clnt = client;
  g_hash_table_foreach(cache, (GHFunc) send_list_entry, &clnt);
}

void forget_old_stuff(char *key, reply_get *value, gpointer user_data)
{
  if (!value->deadline)
    return;
  if (value->deadline < time(NULL))
    delete_secret(key);	 /* FIXME: is this dangerous from inside a foreach? */
  else if (!next_deadline || value->deadline < next_deadline)
    next_deadline = value->deadline;
}

#define HANDLE(signal) if (sigaction(signal, &sa, NULL) < 0) { \
			 fprintf(stderr, \
				 _("could not install %s handler: %s\n"), \
				 #signal, strerror(errno)); \
			 return; \
		       }

/* the main loop - accept connections, serve requests, protect the innocent */
static void agent()
{
  char *req;
  fd_set connections, ready;
  struct sigaction sa;
  int c, nfds;

  sa.sa_handler = exit_gracefully;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  HANDLE(SIGTERM);
  HANDLE(SIGINT);
  HANDLE(SIGHUP);
  sa.sa_handler = SIG_IGN;
  HANDLE(SIGPIPE);
  req = secmem_malloc(MAX_REQUEST_SIZE);
  if (!req) {
    fprintf(stderr, _("could not allocate space in secure storage\n"));
    return;
  }
  cache = g_hash_table_new(g_str_hash, g_str_equal);
  FD_ZERO(&connections);
  FD_SET(sock, &connections);
  nfds = sock + 1;
  while (keep_going) {
    struct timeval tv, *timeout;
    ready = connections;
    while (next_deadline &&
	   (tv.tv_sec = next_deadline - time(NULL)) < 0) {
      next_deadline = 0;	/* compute new deadline */
      g_hash_table_foreach(cache, (GHFunc) forget_old_stuff, NULL);
    }
    if (next_deadline) {
      tv.tv_usec = 0;
      timeout = &tv;
    } else
      timeout = NULL;
    if ((c = select(nfds, &ready, NULL, NULL, timeout)) < 0) {
      if (errno == EINTR)
	continue;
      perror(_("error in select"));
      return;
    } else if (c == 0)
      continue;
    for (c = 0; c < nfds; c++) {
      if (FD_ISSET(c, &ready)) {
	if (c == sock) {
	  int newone;
	  socklen_t size = 0;
	  if ((newone = accept(sock, NULL, &size)) < 0) {
	    perror(_("could not accept connection"));
	    return;
	  }
	  FD_SET(newone, &connections);
	  if (newone >= nfds)
	    nfds = newone + 1;
	} else {
	  int n;
	  switch (n = read(c, req, MAX_REQUEST_SIZE)) {
	  case -1:
	    perror(_("error while receiving"));
				/* fall through */
	  case 0:		/* EOF */
	    close(c);
	    FD_CLR(c, &connections);
	    break;
	  default:
	    debugmsg("read %d bytes on channel %d: ", n, c);
	    if (((request *)req)->magic == REQUEST_MAGIC) {
	      switch (((request *)req)->type)
	      {
	      case REQ_PUT:
		do_put(c, (request_put *)req);
		break;
	      case REQ_GET:
		do_get(c, (request_get *)req);
		break;
	      case REQ_DELETE:
		do_delete(c, (request_get *)req);
		break;
	      case REQ_LIST:
		do_list(c);
		break;
	      default:
		fprintf(stderr, _("malformed message ignored\n"));
	      }
	    } else {
	      fprintf(stderr, _("request with wrong magic number - "
				"maybe an old client?\n"));
	      if (xwrite(c, &failed_reply, sizeof(failed_reply)) < 0)
		perror(_("error while replying"));
	    }
	  }
	}  
      }
    }
  }
  secmem_free(req);
}

int main(int argc, char **argv)
{
  int fd, opt, opt_help = 0, opt_version = 0, opt_fork = 0;
  char *setenv = SETENV_SH;
  struct option opts[] = { { "csh",	no_argument, NULL, 'c' },
			   { "debug",	no_argument, NULL, 'd' },
			   { "fork",	no_argument, &opt_fork, 1 },
			   { "nofork",	no_argument, NULL, 1001 },
			   { "query-options", required_argument, NULL, 'q' },
			   { "help",	no_argument, &opt_help, 1 },
			   { "version", no_argument, &opt_version, 1 },
			   { NULL, 0, NULL, 0 } };

  lower_privs();
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  while ((opt = getopt_long(argc, argv, "cdq:", opts, NULL)) != -1)
    switch (opt) {
    case 'c':
      setenv = SETENV_CSH;
      break;
    case 'd':
      debug++;
      break;
    case 'q':
      query_options = optarg;
      break;
    case 1001:
      fprintf(stderr,
	      _("Warning: not forking is the default now, and --nofork has been deprecated\n"));
      break;
    case 0:
    case '?':
      break;
    default:
      g_error("impossible case in %s:%d", __FILE__, __LINE__);
    }
  if (opt_version) {
    printf("q-agent " VERSION " (" PACKAGE ")\n");
    exit(EXIT_SUCCESS);
  }
  if (opt_help) {
    printf(_("Usage: q-agent [OPTION]...\n\
\n\
  -c, --csh            emit commands compatible with c-shells\n\
  -q, --query-options OPT  pass options OPT through to the query program\n\
  -d, --debug          turn on debugging output\n\
      --fork           fork into the background - keep in mind that this\n\
                       will cause the agent to run until explicitly killed\n\
      --help           display this help and exit\n\
      --version        output version information and exit\n"));
    exit(EXIT_SUCCESS);
  }

  if (create_socket() < 0) {
    cleanup();
    exit(EXIT_FAILURE);
  }
  printf(setenv, sockname);
  fflush(stdout);
  if (opt_fork) {
    switch (fork()) {
    case -1:
      perror(_("could not fork"));
      exit(EXIT_FAILURE);
      break;
    case 0:
      break;
    default:
      exit(EXIT_SUCCESS);
      break;
    }
    if (setsid() < 0) {
      perror(_("could not start new session"));
      exit(EXIT_FAILURE);
    }
  }
  if ((fd = open("/dev/null", O_WRONLY)) < 0) {
    perror(_("could not open /dev/null"));
    exit(EXIT_FAILURE);
  }
  move_fd(fd, STDOUT_FILENO);
  if (!debug)
    xdup2(STDOUT_FILENO, STDERR_FILENO);
  if ((fd = open("/dev/null", O_RDONLY)) < 0) {
    perror(_("could not open /dev/null"));
    exit(EXIT_FAILURE);
  }
  move_fd(fd, STDIN_FILENO);
  raise_privs();
  secmem_init(1);		/* 1 is too small, so default size is used */
  secmem_set_flags(SECMEM_WARN);
  drop_privs();
  supported = 0;
  if ((x_enabled = (getenv("DISPLAY") != NULL)))
    supported |= FLAGS_INSURE;
  agent();
  cleanup();
  exit(EXIT_SUCCESS);
}
