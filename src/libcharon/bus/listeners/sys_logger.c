/*
 * Copyright (C) 2012 Tobias Brunner
 * Copyright (C) 2006 Martin Willi
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "sys_logger.h"

#include <threading/mutex.h>

typedef struct private_sys_logger_t private_sys_logger_t;

/**
 * Private data of a sys_logger_t object
 */
struct private_sys_logger_t {

	/**
	 * Public data.
	 */
	sys_logger_t public;

	/**
	 * syslog facility to use
	 */
	int facility;

	/**
	 * Maximum level to log, for each group
	 */
	level_t levels[DBG_MAX];

	/**
	 * Print the name/# of the IKE_SA?
	 */
	bool ike_name;

	/**
	 * Mutex to ensure multi-line log messages are not torn apart
	 */
	mutex_t *mutex;
};

METHOD(logger_t, log_, void,
	private_sys_logger_t *this, debug_t group, level_t level, int thread,
	ike_sa_t* ike_sa, const char *message)
{
	char groupstr[4], namestr[128] = "";
	const char *current = message, *next;

	/* cache group name and optional name string */
	snprintf(groupstr, sizeof(groupstr), "%N", debug_names, group);

	if (this->ike_name && ike_sa)
	{
		if (ike_sa->get_peer_cfg(ike_sa))
		{
			snprintf(namestr, sizeof(namestr), " <%s|%d>",
				ike_sa->get_name(ike_sa), ike_sa->get_unique_id(ike_sa));
		}
		else
		{
			snprintf(namestr, sizeof(namestr), " <%d>",
				ike_sa->get_unique_id(ike_sa));
		}
	}

	/* do a syslog for every line */
	this->mutex->lock(this->mutex);
	while (TRUE)
	{
		next = strchr(current, '\n');
		if (next == NULL)
		{
			syslog(this->facility | LOG_INFO, "%.2d[%s]%s %s\n",
				   thread, groupstr, namestr, current);
			break;
		}
		syslog(this->facility | LOG_INFO, "%.2d[%s]%s %.*s\n",
			   thread, groupstr, namestr, (int)(next - current), current);
		current = next + 1;
	}
	this->mutex->unlock(this->mutex);
}

METHOD(logger_t, get_level, level_t,
	private_sys_logger_t *this, debug_t group)
{
	return this->levels[group];
}

METHOD(sys_logger_t, set_level, void,
	private_sys_logger_t *this, debug_t group, level_t level)
{
	if (group < DBG_ANY)
	{
		this->levels[group] = level;
	}
	else
	{
		for (group = 0; group < DBG_MAX; group++)
		{
			this->levels[group] = level;
		}
	}
}

METHOD(sys_logger_t, destroy, void,
	private_sys_logger_t *this)
{
	closelog();
	this->mutex->destroy(this->mutex);
	free(this);
}

/*
 * Described in header.
 */
sys_logger_t *sys_logger_create(int facility, bool ike_name)
{
	private_sys_logger_t *this;

	INIT(this,
		.public = {
			.logger = {
				.log = _log_,
				.get_level = _get_level,
			},
			.set_level = _set_level,
			.destroy = _destroy,
		},
		.facility = facility,
		.ike_name = ike_name,
		.mutex = mutex_create(MUTEX_TYPE_DEFAULT),
	);

	set_level(this, DBG_ANY, LEVEL_SILENT);

	return &this->public;
}
