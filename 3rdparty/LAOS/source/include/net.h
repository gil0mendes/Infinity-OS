/*
 * Copyright (C) 2012-2013 Gil Mendes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * @brief		Network boot definitions.
 */

#ifndef __NET_H
#define __NET_H

#include <device.h>

/** Type used to store a MAC address. */
typedef uint8_t mac_addr_t[16];

/** Type used to store an IPv4 address. */
typedef uint8_t ipv4_addr_t[4];

/** Type used to store an IPv6 address. */
typedef uint8_t ipv6_addr_t[16];

/** Type used to store an IP address. */
typedef union ip_addr {
	ipv4_addr_t v4;			/**< IPv4 address. */
	ipv6_addr_t v6;			/**< IPv6 address. */
} ip_addr_t;

/** Structure containing details of a network boot server. */
typedef struct net_device {
	device_t device;		/**< Device header. */

	uint32_t flags;			/**< Behaviour flags. */
	ip_addr_t server_ip;		/**< Server IP address. */
	uint16_t server_port;		/**< UDP port number of TFTP server. */
	ip_addr_t gateway_ip;		/**< Gateway IP address. */
	ip_addr_t client_ip;		/**< IP used on this machine when communicating with server. */
	mac_addr_t client_mac;		/**< MAC address of the boot network interface. */
	uint8_t hw_type;		/**< Hardware type (according to RFC 1700). */
	uint8_t hw_addr_len;		/**< Hardware address length. */
} net_device_t;

/** Network device flags. */
#define NET_DEVICE_IPV6		(1<<0)	/**< Device is configured using IPv6. */

#endif /* __NET_H */
