/*
 * Copyright (C) 2013 Gil Mendes
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Inter-Process Communication (IPC) interface.
 */

#ifndef __KERNEL_IPC_H
#define __KERNEL_IPC_H

#include <kernel/object.h>
#include <kernel/security.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of data that can be attached to a message. */
#define IPC_DATA_MAX			16384

/** Maximum number of messages that can be queued at a time. */
#define IPC_QUEUE_MAX			256

/** Structure describing an IPC message. */
typedef struct ipc_message {
	uint32_t id;
	uint16_t flags;				/**< Message flags. */
	uint16_t size;				/**< Size of attached data. */
	unsigned long args[6];			/**< Inline message arguments. */
} ipc_message_t;

/** IPC message flags. */
#define IPC_MESSAGE_VALID		(1<<0)	/**< Message is valid (ignored when sending). */
#define IPC_MESSAGE_HANDLE		(1<<1)	/**< Message has an attached handle. */

/** Structure describing an IPC client. */
typedef struct ipc_client {
	process_id_t pid;			/**< Connecting process ID. */
	security_context_t security;		/**< Security context at time of connection. */
} ipc_client_t;

/** IPC port event IDs. */
#define PORT_EVENT_LISTEN		0	/**< A connection is being made to the port. */

/** IPC connection event IDs. */
#define CONNECTION_EVENT_HANGUP		0	/**< Remote end hung up or port was deleted. */
#define CONNECTION_EVENT_RECEIVE	1	/**< A message is received. */

/** Special process port IDs (negative values to distinguish from handles). */
#define PROCESS_ROOT_PORT		(-1)

/** Thread special port IDs (must not conflict with process IDs). */
#define THREAD_EXCEPTION_PORT		(-10)

/** Behaviour flags for kern_port_listen(). */
#define PORT_LISTEN_ACCEPT		(1<<0)	/**< Immediately accept the connection. */

extern status_t kern_port_create(handle_t *handlep);
extern status_t kern_port_listen(handle_t handle, unsigned flags,
	ipc_message_t *payload, ipc_client_t *client, nstime_t timeout,
	handle_t *handlep);

extern status_t kern_connection_open(handle_t port,
	const ipc_message_t *payload, const void *data, handle_t attached,
	nstime_t timeout, handle_t *handlep);

extern status_t kern_connection_accept(handle_t handle);
extern status_t kern_connection_reject(handle_t handle, status_t status);
extern status_t kern_connection_forward(handle_t handle, handle_t port,
	const ipc_message_t *payload, const void *data, handle_t attached);

extern status_t kern_connection_send(handle_t handle, const ipc_message_t *msg,
	const void *data, handle_t attached, nstime_t timeout);
extern status_t kern_connection_receive(handle_t handle, ipc_message_t *msg,
	nstime_t timeout);
extern status_t kern_connection_receive_data(handle_t handle, void *data);
extern status_t kern_connection_receive_handle(handle_t handle,
	handle_t *attachedp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_IPC_H */