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

#ifndef _AGENTLIB_H
#define _AGENTLIB_H

#include "agent.h"

int agent_init();
int agent_done();
status_t agent_list();
status_t agent_put(const char *id, const flags_t flags, const time_t deadline,
		   const char *comment, const char *data);
status_t agent_get(const char *id, reply_get **reply);
status_t agent_delete(const char *id);

#endif
