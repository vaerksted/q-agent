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

#ifndef _AGENT_H
#define _AGENT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else  /* HAVE_INTTYPES_H */
#if   SIZEOF_UNSIGNED_INT  == 4
typedef unsigned int uint32_t;
#elif SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long uint32_t;
#else
#error No unsigned type of width 32 known
#endif
#endif /* HAVE_INTTYPES_H */

#define SOCKET_NAME	"agent-socket"

#define ID_LENGTH	100
#define COMMENT_LENGTH	100
#define DATA_LENGTH	1000

/* magic numbers to identify genuine request; these should change,
   whenever the request/reply format changes. */
#define REQUEST_MAGIC	0xa8e42301
#define REPLY_MAGIC	0xa8f42301

/* request types */
typedef enum _req_type {
  REQ_PUT, REQ_GET, REQ_DELETE, REQ_LIST
} req_type;

typedef int flags_t;
#define FLAGS_INSURE	1	/* whether the agent should ask before
				   handing out secrets */

/* generic part of requests */
typedef struct _request {
  uint32_t magic;		/* magic number */
  req_type type;		/* request type */
} request;

/* PUT request: store the secret <data> and <comment> under <id> */
typedef struct _request_put {
  uint32_t magic;		/* magic number */
  req_type type;		/* request type */
  char id[ID_LENGTH];		/* identifier of the secret */
  flags_t flags;		/* miscellaneous flags - see above */
  time_t deadline;		/* will forget after this deadline */
  char comment[COMMENT_LENGTH];	/* human-readable comment attached to secret */
  char data[DATA_LENGTH];	/* the secret itself */
} request_put;

/* GET request: retrieve the secret <id> */
/* DELETE request: delete the secret <id> (format equals GET request) */
typedef struct _request_get {
  uint32_t magic;		/* magic number */
  req_type type;		/* request type */
  char id[ID_LENGTH];		/* identifier of the secret */
} request_get, request_delete;

#define MAX_REQUEST_SIZE	(sizeof(request_put))

typedef enum _status_t {
  STATUS_OK, STATUS_FAIL, STATUS_COMM_ERR
} status_t;

/* generic part of replies */
/* replies to PUT and DELETE just have the generic part */
typedef struct _reply {
  uint32_t magic;		/* magic number */
  status_t status;		/* whether the request succeeded */
} reply, reply_put, reply_delete;

/* reply to GET request */
typedef struct _reply_get {
  uint32_t magic;		/* magic number */
  status_t status;		/* whether the request succeeded */
  flags_t flags;		/* miscellaneous flags - see above */
  time_t deadline;		/* will forget after this deadline */
  char comment[COMMENT_LENGTH];	/* human-readable comment attached to secret */
  char data[DATA_LENGTH];	/* the secret itself */
} reply_get;

/* a single entry within the LIST reply */
typedef struct _reply_list_entry {
  char id[ID_LENGTH];		/* identifier of the secret */
  flags_t flags;		/* miscellaneous flags - see above */
  time_t deadline;		/* will forget after this deadline */
  char comment[COMMENT_LENGTH];	/* human-readable comment attached to secret */
} reply_list_entry;

/* reply to LIST request */
typedef struct _reply_list {
  uint32_t magic;		/* magic number */
  status_t status;		/* whether the request succeeded */
  unsigned entries;		/* number of entries that follow */
  reply_list_entry entry[0]; /* the dark entries */
} reply_list;

#endif
