/* Quintuple Agent client test
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
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#ifndef HAVE_STRDUP
#include "strdup.h"
#endif
#ifndef HAVE_SETENV
#include "setenv.h"
#endif

#define SHELL		"/bin/sh"
#define AGENT_CMD	"../q-agent"
#define CLIENT_CMD	"../q-client "
#define FILTER_CMD	"grep -v '^Warning: using insecure memory!$'"
#define DIFF_CMD	"diff -c"

pid_t agent_pid;

/* substr - return a new copy of the string in [START,END). */
char *substr(char *start, char *end)
{
  size_t len;
  char *sub;

  if (!end)
    return strdup(start);
  len = end - start;
  sub = (char *)malloc(len + 1);
  if (!sub) {
    perror("couldn't allocate new string");
    exit(EXIT_FAILURE);
  }
  strncpy(sub, start, len);
  sub[len] = 0;
  return sub;
}

void timeout(int sig)
{
  kill(agent_pid, SIGKILL);
}

void stop_agent()
{
  kill(agent_pid, SIGTERM);
  signal(SIGALRM, timeout);
  alarm(5);
  if (waitpid(agent_pid, NULL, 0) < 0) {
    perror("couldn't wait for shutdown of q-agent");
  }
  alarm(0);
}

void remove_files()
{
  unlink("client1.out");
  unlink("client2.out");
  unlink("diff.out");
}

void start_agent()
{
  int p[2];
  char buf[BUFSIZ], *s;

  if (pipe(p) < 0) {
    perror("couldn't create pipe");
    exit(EXIT_FAILURE);
  }
  if ((agent_pid = fork()) < 0) {
    perror("couldn't fork");
    exit(EXIT_FAILURE);
  }
  if (agent_pid == 0) {
    close(p[0]);
    if (dup2(p[1], STDOUT_FILENO) < 0) {
      perror("couldn't dup2");
      exit(EXIT_FAILURE);
    }
    close(p[1]);
    execl(AGENT_CMD, "q-agent", NULL);
    perror("couldn't exec `q-agent'");
    exit(EXIT_FAILURE);
  }
  close(p[1]);
  atexit(stop_agent);
  if (read(p[0], buf, BUFSIZ) <= 0) {
    perror("couldn't read agent output");
    exit(EXIT_FAILURE);
  }
  if (!strncmp(buf, "AGENT_SOCKET='", 14)
      && (s = strstr(buf+14, "'; export AGENT_SOCKET")) != NULL) {
    s[0] = 0;
    setenv("AGENT_SOCKET", buf+14, 1);
  } else {
    fprintf(stderr, "couldn't parse agent output: %s", buf);
    exit(EXIT_FAILURE);
  }
}

void client(char *args, char *in, char *out, int stat)
{
  int status;
  char *buf;
  FILE *client, *diff;

#define CLIENT_CMD2 (CLIENT_CMD " >client1.out ")
  
  buf = malloc(strlen(CLIENT_CMD2) + strlen(args) + 1);
  strcpy(buf, CLIENT_CMD2);
  strcat(buf, args);
  if (!(client = popen(buf, "w"))) {
    perror("couldn't popen client");
    free(buf);
    exit(EXIT_FAILURE);
  }
  free(buf);
  printf("Testing %-30s ... ", args);
  if (in)
    if (fprintf(client, "%s", in) < 0) {
      perror("couldn't write to client");
      exit(EXIT_FAILURE);
    }
  if ((status = pclose(client)) < 0) {
    perror("FAIL: couldn't finish client");
    exit(EXIT_FAILURE);
  }
  if (!WIFEXITED(status)) {
    printf("FAIL: client died abnormally\n");
    exit(EXIT_FAILURE);
  }
  if (WEXITSTATUS(status) != stat) {
    printf("FAIL: exit status %d instead of %d\n", WEXITSTATUS(status), stat);
    exit(EXIT_FAILURE);
  }
  if (out) {
    if (system(FILTER_CMD " < client1.out > client2.out") < 0) {
      perror("couldn't exec " FILTER_CMD);
      exit(EXIT_FAILURE);
    }
    if (!(diff = popen(DIFF_CMD " - client2.out > diff.out", "w"))) {
      perror("couldn't popen " DIFF_CMD);
      exit(EXIT_FAILURE);
    }
    if (fprintf(diff, "%s", out) < 0) {
      perror("couldn't write to diff");
      exit(EXIT_FAILURE);
    }
    if ((status = pclose(diff)) < 0) {
      perror("couldn't finish diff");
      exit(EXIT_FAILURE);
    }
    if (!WIFEXITED(status)) {
      printf("diff died abnormally\n");
      exit(EXIT_FAILURE);
    }
    if (WEXITSTATUS(status) != 0) {
      int fd;
      printf("FAIL: diff output mismatch\n");
      if ((fd = open("diff.out", O_RDONLY)) >= 0) {
	char buf[BUFSIZ];
	ssize_t n;
	while ((n = read(fd, buf, BUFSIZ)) > 0)
	  write(STDOUT_FILENO, buf, n);
	close(fd);
      }
      exit(EXIT_FAILURE);
    }
  }
  printf("PASS\n");
}

int main()
{
  time_t deadline;

  unsetenv("DISPLAY");
  setenv("LANG", "C", 1);
  start_agent();
  atexit(remove_files);
  client("list", NULL, "", 0);
  client("put 23 \"Joe Malik\"", "fnord\n", NULL, 0);
  client("list", NULL, "23\tnone                \t\tJoe Malik\n", 0);
  client("get 23", NULL, "fnord\n", 0);
  client("get foo", NULL, "", 2);
  client("-t 3 put 17 \"J. Random Hacker\"", "fubar\n", NULL, 0);
  deadline = time(NULL) + 3 + 1;
  client("get 17", NULL, "fubar\n", 0);
  while (time(NULL) < deadline)
    sleep(deadline - time(NULL));
  client("get 17", NULL, "", 2);
  client("delete 23", NULL, NULL, 0);
  client("get 23", NULL, "", 2);
  client("delete 23", NULL, "", 0);
  return EXIT_SUCCESS;
}
