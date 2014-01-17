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
 * @brief		PXE functions/definitions.
 */

#ifndef __PLATFORM_PXE_H
#define __PLATFORM_PXE_H

#include <types.h>

/** PXE function numbers. */
#define PXENV_UNDI_SHUTDOWN		0x0005	/**< Reset the network adapter. */
#define PXENV_STOP_UNDI			0x0015	/**< Shutdown the UNDI stack. */
#define PXENV_TFTP_OPEN			0x0020	/**< Open TFTP connection. */
#define PXENV_TFTP_CLOSE		0x0021	/**< Close TFTP connection. */
#define PXENV_TFTP_READ			0x0022	/**< Read from TFTP connection. */
#define PXENV_TFTP_GET_FSIZE		0x0025	/**< Get TFTP file size. */
#define PXENV_UNLOAD_STACK		0x0070	/**< Unload PXE stack. */
#define PXENV_GET_CACHED_INFO		0x0071	/**< Get cached information. */

/** Packet types for PXENV_GET_CACHED_INFO. */
#define PXENV_PACKET_TYPE_DHCP_DISCOVER	1	/**< Get DHCPDISCOVER packet. */
#define PXENV_PACKET_TYPE_DHCP_ACK	2	/**< Get DHCPACK packet. */
#define PXENV_PACKET_TYPE_CACHED_REPLY	3	/**< Get DHCP reply packet. */

/** Return codes from PXE calls. */
#define PXENV_EXIT_SUCCESS		0x0000	/**< Success. */
#define PXENV_EXIT_FAILURE		0x0001	/**< Failure. */

/** TFTP connection settings. */
#define PXENV_TFTP_PORT			69	/**< Port number. */
#define PXENV_TFTP_PACKET_SIZE		512	/**< Requested packet size. */

/** Type containing a segment/offset. */
typedef union pxe_segoff {
	uint32_t addr;
	struct {
		uint16_t offset;		/**< Offset. */
		uint16_t segment;		/**< Segment. */
	} __packed;
} pxe_segoff_t;

/** Type of an IPv4 address. */
typedef union pxe_ip4 {
	uint32_t n;
	uint8_t a[4];
} pxe_ip4_t;

/** Type of a MAC address. */
typedef uint8_t pxe_mac_addr_t[16];

/** Type of a PXENV status code. */
typedef uint16_t pxenv_status_t;

/** PXENV+ structure. */
typedef struct pxenv {
	uint8_t signature[6];			/**< Signature. */
	uint16_t version;			/**< API version number. */
	uint8_t length;				/**< Length of the structure. */
	uint8_t checksum;			/**< Checksum. */
	pxe_segoff_t rm_entry;			/**< Real mode entry point. */
	uint32_t pm_entry;			/**< Protected mode entry point. */
	uint16_t pm_selector;			/**< Protected mode segment selector. */
	uint16_t stack_seg;			/**< Stack segment. */
	uint16_t stack_size;			/**< Stack segment size. */
	uint16_t bc_code_seg;			/**< BC code segment. */
	uint16_t bc_code_size;			/**< BC code segment size. */
	uint16_t bc_data_seg;			/**< BC data segment. */
	uint16_t bc_data_size;			/**< BC data segment size. */
	uint16_t undi_code_seg;			/**< UNDI code segment. */
	uint16_t undi_code_size;		/**< UNDI code segment size. */
	uint16_t undi_data_seg;			/**< UNDI data segment. */
	uint16_t undi_data_size;		/**< UNDI data segment size. */
	pxe_segoff_t pxe_ptr;			/**< Pointer to !PXE structure. */
} __packed pxenv_t;

/** !PXE structure. */
typedef struct pxe {
	uint8_t signature[4];			/**< Signature. */
	uint8_t length;				/**< Structure length. */
	uint8_t checksum;			/**< Checksum. */
	uint8_t revision;			/**< Structure revision. */
	uint8_t reserved1;			/**< Reserved. */
	pxe_segoff_t undi_rom_id;		/**< Address of UNDI ROM ID structure. */
	pxe_segoff_t base_rom_id;		/**< Address of BC ROM ID structure. */
	pxe_segoff_t entry_point_16;		/**< Entry point for 16-bit stack segment. */
	pxe_segoff_t entry_point_32;		/**< Entry point for 32-bit stack segment. */
	pxe_segoff_t status_callout;		/**< Status call-out function. */
	uint8_t reserved2;			/**< Reserved. */
	uint8_t seg_desc_count;			/**< Number of segment descriptors. */
	uint16_t first_selector;		/**< First segment selector. */
	uint8_t segments[56];			/**< Segment information. */
} __packed pxe_t;

/** Input structure for PXENV_TFTP_OPEN. */
typedef struct pxenv_tftp_open {
	pxenv_status_t status;			/**< Status code. */
	pxe_ip4_t server_ip;			/**< Server IP address. */
	pxe_ip4_t gateway_ip;			/**< Gateway IP address. */
	uint8_t filename[128];			/**< File name to open. */
	uint16_t udp_port;			/**< Port that TFTP server is listening on. */
	uint16_t packet_size;			/**< Requested packet size. */
} __packed pxenv_tftp_open_t;

/** Input structure for PXENV_TFTP_CLOSE. */
typedef struct pxenv_tftp_close {
	pxenv_status_t status;			/**< Status code. */
} __packed pxenv_tftp_close_t;

/** Input structure for PXENV_TFTP_READ. */
typedef struct pxenv_tftp_read {
	pxenv_status_t status;			/**< Status code. */
	uint16_t packet_number;			/**< Packet number sent by server. */
	uint16_t buffer_size;			/**< Number of bytes read. */
	pxe_segoff_t buffer;			/**< Buffer address. */
} __packed pxenv_tftp_read_t;

/** Input structure for PXENV_TFTP_GET_FSIZE. */
typedef struct pxenv_tftp_get_fsize {
	pxenv_status_t status;			/**< Status code. */
	pxe_ip4_t server_ip;			/**< Server IP address. */
	pxe_ip4_t gateway_ip;			/**< Gateway IP address. */
	uint8_t filename[128];			/**< File name to open. */
	uint32_t file_size;			/**< Size of the file. */
} __packed pxenv_tftp_get_fsize_t;

/** Input structure for PXENV_GET_CACHED_INFO. */
typedef struct pxenv_get_cached_info {
	pxenv_status_t status;			/**< Status code. */
	uint16_t packet_type;			/**< Requested packet. */
	uint16_t buffer_size;			/**< Size of output buffer. */
	pxe_segoff_t buffer;			/**< Buffer address. */
	uint16_t buffer_limit;			/**< Maximum size of buffer. */
} __packed pxenv_get_cached_info_t;

/** Cached packet structure. */
typedef struct pxenv_boot_player {
	uint8_t opcode;				/**< Message opcode. */
	uint8_t hardware;			/**< Hardware type. */
	uint8_t hardware_len;			/**< Hardware address length. */
	uint8_t gate_hops;
	uint32_t ident;				/**< Random number chosen by client. */
	uint16_t seconds;			/**< Seconds since obtained address. */
	uint16_t flags;				/**< BOOTP/DHCP flags. */
	pxe_ip4_t client_ip;			/**< Client IP. */
	pxe_ip4_t your_ip;			/**< Your IP. */
	pxe_ip4_t server_ip;			/**< Server IP. */
	pxe_ip4_t gateway_ip;			/**< Gateway IP. */
	pxe_mac_addr_t client_addr;		/**< Client hardware address. */
	uint8_t server_name[64];		/**< Server host name. */
	uint8_t boot_file[128];			/**< Boot file name. */
	uint8_t vendor[64];			/**< DHCP vendor options. */
} __packed pxenv_boot_player_t;

extern pxe_segoff_t pxe_entry_point;

extern bool pxe_detect(void);

#endif /* __PLATFORM_PXE_H */
