/*
 * Copyright (C) 2012 Tobias Brunner
 * Copyright (C) 2012 Giuliano Grassi
 * Copyright (C) 2012 Ralf Sager
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

/**
 * @defgroup tun_device tun_device
 * @{ @ingroup utils
 */

#ifndef TUN_DEVICE_H_
#define TUN_DEVICE_H_

#include <library.h>
#include <utils/host.h>

typedef struct tun_device_t tun_device_t;

/**
 * Class to create TUN devices
 *
 * Creating such a device requires the CAP_NET_ADMIN capability.
 *
 * @note The implementation is currently very Linux specific
 */
struct tun_device_t {

	/**
	 * Read a packet from the TUN device
	 *
	 * @note This call blocks until a packet is available. It is a thread
	 * cancellation point.
	 *
	 * @param packet		the packet read from the device
	 * @return				TRUE if successful
	 */
	bool (*read_packet)(tun_device_t *this, chunk_t *packet);

	/**
	 * Write a packet to the TUN device
	 *
	 * @param packet		the packet to write to the TUN device
	 * @return				TRUE if successful
	 */
	bool (*write_packet)(tun_device_t *this, chunk_t packet);

	/**
	 * Set the IP address of the device
	 *
	 * @param addr			the desired interface address
	 * @param netmask		the netmask to use
	 * @return				TRUE if operation successful
	 */
	bool (*set_address)(tun_device_t *this, host_t *addr, u_int8_t netmask);

	/**
	 * Bring the TUN device up
	 *
	 * @return				TRUE if operation successful
	 */
	bool (*up)(tun_device_t *this);

	/**
	 * Set the MTU for this TUN device
	 *
	 * @param mtu			new MTU
	 * @return				TRUE if operation successful
	 */
	bool (*set_mtu)(tun_device_t *this, int mtu);

	/**
	 * Get the current MTU for this TUN device
	 *
	 * @return				current MTU
	 */
	int (*get_mtu)(tun_device_t *this);

	/**
	 * Get the interface name of this device
	 *
	 * @return				interface name
	 */
	char *(*get_name)(tun_device_t *this);

	/**
	 * Destroy a tun_device_t
	 */
	void (*destroy)(tun_device_t *this);

};

/**
 * Create a TUN device using the given name template.
 *
 * @param name_tmpl			name template, defaults to "tun%d" if not given
 * @return					TUN device
 */
tun_device_t *tun_device_create(const char *name_tmpl);

#endif /** TUN_DEVICE_H_ @}*/
