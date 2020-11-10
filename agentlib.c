/* Quintuple Agent client library
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
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>	/* must be before sys/stat.h due to Ultrix-breakage */
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "i18n.h"
#include "agentlib.h"
#include "util.h"
#include "memory.h"

static int sock = -1;

int agent_init()
{
  char *sockname;
  struct sockaddr_un *addr;
  socklen_t len;

  if (sock != -1)
    return 0;
  if ((sockname = getenv("AGENT_SOCKET")) == NULL) {
    fprintf(stderr, _("AGENT_SOCKET is not set\n"));
    /* IDEA: should we try guessing sockets? */
    return -1;
  }
  if (!(sock = socket(PF_UNIX, SOCK_STREAM, 0))) {
    perror(_("could not create socket"));
    return -1;
  }
  len = offsetof(struct sockaddr_un, sun_path) + strlen(sockname) + 1;
  addr = alloca(len);
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, sockname);
  if (connect(sock, (struct sockaddr *)addr, len) < 0) {
    perror(_("could not connect to server"));
    return -1;
  }
  return 0;
}

int agent_done()
{
  int ret;

  ret = close(sock);
  sock = -1;
  return ret;
}

static status_t
send_request(request *req, size_t size, reply **re, size_t *rsize)
{
  static reply tmp;
  reply *r;
  size_t s;
  ssize_t n;

  req->magic = REQUEST_MAGIC;
  if (re) {
    r = *re;
    s = *rsize;
  } else {
    r = &tmp;
    s = sizeof(reply);
  }
  if (xwrite(sock, req, size) >= 0) {
    if ((n = read(sock, r, s)) >= 0) {
      if (r->magic == REPLY_MAGIC) {
	return r->status;
      } else {
	fprintf(stderr,
		_("wrong magic number on reply - maybe your agent is old?\n"));
	return r->status = STATUS_COMM_ERR;
      }
    } else
      perror(_("error receiving reply"));
  } else
    perror(_("could not send request"));
  return r->status = STATUS_COMM_ERR;
}

status_t agent_list(reply_list **rep)
{
  status_t ret;
  request req;
  size_t rs;

  req.type = REQ_LIST;
  rs = sizeof(reply_list);
  ret = send_request(&req, sizeof(req), (reply **)rep, &rs);
  if (ret == STATUS_OK && (*rep)->entries) {
    unsigned i;
    *rep =
      realloc(*rep, sizeof(reply_list)
	      + (*rep)->entries * sizeof(reply_list_entry));
    if (!*rep) {
      fprintf(stderr, _("out of memory\n"));
      return STATUS_FAIL;
    }
    for (i=0; i < (*rep)->entries; i++) {
      if (read(sock, (*rep)->entry + i, sizeof(reply_list_entry))
	  != sizeof(reply_list_entry)) {
	perror(_("error receiving reply"));
	break;
      } 
    }
  }
  return ret;
}

status_t agent_put(const char *id, const flags_t flags, const time_t deadline,
		   const char *comment, const char *data)
{
  status_t ret;
  request_put *req;

  if (!(req = secmem_malloc(sizeof(request_put)))) {
    fprintf(stderr, _("could not allocate space in secure storage\n"));
    return STATUS_FAIL;
  }
  req->type = REQ_PUT;
  strcpy(req->id, id);
  req->flags = flags;
  req->deadline = deadline;
  strcpy(req->comment, comment);
  strcpy(req->data, data);
  ret = send_request((request *)req, sizeof(request_put), NULL, 0);
  secmem_free(req);
  return ret;
}

status_t agent_get(const char *id, reply_get **rep)
{
  request_get req;
  size_t rs;

  req.type = REQ_GET;
  strcpy(req.id, id);
  rs = sizeof(reply_get);
  if (!(*rep = secmem_malloc(rs))) {
    fprintf(stderr, _("could not allocate space in secure storage\n"));
    return STATUS_FAIL;
  }
  return send_request((request *)&req, sizeof(req), (reply **)rep, &rs);
}

status_t agent_delete(const char *id)
{
  request_get req;

  req.type = REQ_DELETE;
  strcpy(req.id, id);
  return send_request((request *)&req, sizeof(req), NULL, 0);
}
