/*
 * Copyright (C) 2013-2014 Gil Mendes
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
 * @brief		IPC interface.
 *
 * @todo		We don't handle 0 timeout in user_ipc_port_connect()
 *			properly.
 */

#include <ipc/ipc.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <security/security.h>

#include <assert.h>
#include <kdb.h>
#include <status.h>

/** Define to enable debug output. */
//#define DEBUG_IPC

#ifdef DEBUG_IPC
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Caches for IPC structures. */
static slab_cache_t *ipc_port_cache;
static slab_cache_t *ipc_connection_cache;
static slab_cache_t *ipc_kmessage_cache;

/** Constructor for port objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void ipc_port_ctor(void *obj, void *data) {
	ipc_port_t *port = obj;

	mutex_init(&port->lock, "ipc_port_lock", 0);
	list_init(&port->waiting);
	condvar_init(&port->listen_cvar, "ipc_port_listen");
	notifier_init(&port->connection_notifier, port);
}

/** Disown an IPC port.
 * @param port		Port to disown (must be locked). */
static void ipc_port_disown(ipc_port_t *port) {
	ipc_connection_t *conn;

	port->owner = NULL;

	/* Cancel all in-progress connection attempts. */
	LIST_FOREACH_SAFE(&port->waiting, iter) {
		conn = list_entry(iter, ipc_connection_t, header);

		mutex_lock(&conn->lock);

		list_remove(&conn->header);
		conn->state = IPC_CONNECTION_CLOSED;
		condvar_broadcast(&conn->open_cvar);

		mutex_unlock(&conn->lock);
	}
}

/** Close a handle to a port.
 * @param handle	Handle to the port. */
static void port_object_close(object_handle_t *handle) {
	ipc_port_release(handle->private);
}

/** Called when a port handle is attached to a process.
 * @param handle	Handle to the port.
 * @param process	Process the handle is being attached to. */
static void port_object_attach(object_handle_t *handle, process_t *process) {
	ipc_port_t *port = handle->private;

	mutex_lock(&port->lock);

	if(process == port->owner)
		port->owner_count++;

	mutex_unlock(&port->lock);
}

/** Called when a port handle is detached from a process.
 * @param handle	Handle to the port.
 * @param process	Process the handle is being detached from. */
static void port_object_detach(object_handle_t *handle, process_t *process) {
	ipc_port_t *port = handle->private;

	mutex_lock(&port->lock);

	if(process == port->owner && --port->owner_count == 0) {
		ipc_port_disown(port);

		dprintf("ipc: process %" PRId32 " disowned port %p\n", process->id, port);
	}

	mutex_unlock(&port->lock);
}

/**
* Signal that a port event is being waited for.
*
* @param handle	    Handle to the port.
* @param event		Event that is being waited for.
*
* @return		    Status code describing result of the operation.
*/
static status_t
port_object_wait(object_handle_t *handle, object_event_t *event)
{
	ipc_port_t *port = handle->private;
	status_t ret;

	mutex_lock(&port->lock);

	switch(event->event) {
	case PORT_EVENT_CONNECTION:
		if(curr_proc != port->owner) {
			mutex_unlock(&port->lock);
			return STATUS_ACCESS_DENIED;
		}

		if(!list_empty(&port->waiting)) {
			object_event_signal(event, 0);
		} else {
			notifier_register(&port->connection_notifier, object_event_notifier, event);
		}

		ret = STATUS_SUCCESS;
		break;
	default:
		ret = STATUS_INVALID_EVENT;
		break;
	}

	mutex_unlock(&port->lock);
	return ret;
}

/**
* Stop waiting for a port event.
*
* @param handle	Handle to the port.
* @param event		Event that is being waited for.
*/
static void
port_object_unwait(object_handle_t *handle, object_event_t *event)
{
	ipc_port_t *port = handle->private;

	switch(event->event) {
	case PORT_EVENT_CONNECTION:
		notifier_unregister(&port->connection_notifier, object_event_notifier, event);
		break;
	}
}

/** Port object type. */
static object_type_t port_object_type = {
	.id = OBJECT_TYPE_PORT,
	.flags = OBJECT_TRANSFERRABLE,
	.close = port_object_close,
	.attach = port_object_attach,
	.detach = port_object_detach,
	.wait = port_object_wait,
	.unwait = port_object_unwait,
};

/** Constructor for connection objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void ipc_connection_ctor(void *obj, void *data) {
	ipc_connection_t *conn = obj;
	ipc_endpoint_t *endpoint;
	size_t i;

	mutex_init(&conn->lock, "ipc_connection_lock", 0);
	condvar_init(&conn->open_cvar, "ipc_connection_open");
	list_init(&conn->header);

	conn->endpoints[0].remote = &conn->endpoints[1];
	conn->endpoints[1].remote = &conn->endpoints[0];

	for(i = 0; i < 2; i++) {
		endpoint = &conn->endpoints[i];
		endpoint->conn = conn;
		endpoint->message_count = 0;
		endpoint->pending = NULL;

		list_init(&endpoint->messages);
		condvar_init(&endpoint->space_cvar, "ipc_connection_send");
		condvar_init(&endpoint->data_cvar, "ipc_connection_receive");
		notifier_init(&endpoint->hangup_notifier, endpoint);
		notifier_init(&endpoint->message_notifier, endpoint);
	}
}

/** Release an IPC connection.
 * @param conn		Connection to release. */
static void ipc_connection_release(ipc_connection_t *conn) {
	assert(conn->state == IPC_CONNECTION_CLOSED);

	if(refcount_dec(&conn->count) > 0)
		return;

	dprintf("ipc: destroying connection %p\n", conn);

	/* Message queues should be emptied by ipc_connection_close(). */
	assert(list_empty(&conn->endpoints[0].messages));
	assert(!conn->endpoints[0].pending);
	assert(list_empty(&conn->endpoints[1].messages));
	assert(!conn->endpoints[1].pending);

	slab_cache_free(ipc_connection_cache, conn);
}

/** Close a handle to a connection.
 * @param handle	Handle to the connection. */
static void connection_object_close(object_handle_t *handle) {
	ipc_connection_close(handle->private);
}

/**
* Signal that a connection event is being waited for.
*
* @param handle	    Handle to the connection.
* @param event		Event that is being waited for.
*
* @return		    Status code describing result of the operation.
*/
static status_t
connection_object_wait(object_handle_t *handle, object_event_t *event)
{
	ipc_endpoint_t *endpoint = handle->private;
	status_t ret;

	mutex_lock(&endpoint->conn->lock);

	switch(event->event) {
	case CONNECTION_EVENT_HANGUP:
        if(!(event->flags & OBJECT_EVENT_EDGE) && endpoint->conn->state == IPC_CONNECTION_CLOSED) {
			object_event_signal(event, 0);
		} else {
			notifier_register(&endpoint->hangup_notifier, object_event_notifier, event);
		}

		ret = STATUS_SUCCESS;
		break;
	case CONNECTION_EVENT_MESSAGE:
        if(!(event->flags & OBJECT_EVENT_EDGE) && endpoint->message_count) {
			object_event_signal(event, 0);
		} else {
			notifier_register(&endpoint->message_notifier, object_event_notifier, event);
		}

		ret = STATUS_SUCCESS;
		break;
	default:
		ret = STATUS_INVALID_EVENT;
		break;
	}

	mutex_unlock(&endpoint->conn->lock);
	return ret;
}

/**
* Stop waiting for a connection event.
*
* @param handle	Handle to the connection.
* @param event		Event that is being waited for.
*/
static void
connection_object_unwait(object_handle_t *handle, object_event_t *event)
{
	ipc_endpoint_t *endpoint = handle->private;

	switch(event->event) {
	case CONNECTION_EVENT_HANGUP:
		notifier_unregister(&endpoint->hangup_notifier, object_event_notifier, event);
		break;
	case CONNECTION_EVENT_MESSAGE:
		notifier_unregister(&endpoint->message_notifier, object_event_notifier, event);
		break;
	}
}

/** Connection object type. */
static object_type_t connection_object_type = {
	.id = OBJECT_TYPE_CONNECTION,
	.close = connection_object_close,
	.wait = connection_object_wait,
	.unwait = connection_object_unwait,
};

/** Queue a message at an endpoint.
 * @param conn		Connection being sent on (must be locked).
 * @param endpoint	Remote endpoint to queue at.
 * @param msg		Message to queue.
 * @param flags		Behaviour flags.
 * @param timeout	Timeout in nanoseconds.
 * @return		Status code describing result of the operation. */
static status_t
queue_message(ipc_connection_t *conn, ipc_endpoint_t *endpoint,
	ipc_kmessage_t *msg, unsigned flags, nstime_t timeout)
{
	nstime_t absolute;
	unsigned sleep;
	status_t ret;

	assert(conn->state != IPC_CONNECTION_SETUP);

	if(conn->state == IPC_CONNECTION_CLOSED)
		return STATUS_CONN_HUNGUP;

	if(endpoint->flags & IPC_ENDPOINT_DROP)
		return STATUS_SUCCESS;

	/* Save the message timestamp and security context. */
	msg->msg.timestamp = system_time();
	memcpy(&msg->security, security_current_context(), sizeof(msg->security));

	/* Wait for queue space if we're not forcing the send. */
	if(!(flags & IPC_FORCE)) {
		absolute = (timeout > 0) ? system_time() + timeout : timeout;
		sleep = SLEEP_ABSOLUTE
			| ((flags & IPC_INTERRUPTIBLE) ? SLEEP_INTERRUPTIBLE : 0);

		while(endpoint->message_count >= IPC_QUEUE_MAX) {
			ret = condvar_wait_etc(&endpoint->space_cvar, &conn->lock,
				absolute, sleep);

			/* Connection could have been closed while we were
			 * waiting (see ipc_connection_close()). */
			if(conn->state == IPC_CONNECTION_CLOSED)
				return STATUS_CONN_HUNGUP;

			if(ret != STATUS_SUCCESS) {
				if(endpoint->message_count >= IPC_QUEUE_MAX)
					return ret;
			}
		}
	}

	/* Queue the message. */
	refcount_inc(&msg->count);
	list_append(&endpoint->messages, &msg->header);
	endpoint->message_count++;
	condvar_signal(&endpoint->data_cvar);
	notifier_run(&endpoint->message_notifier, NULL, false);
	return STATUS_SUCCESS;
}

/** Receive a message on an endpoint.
 * @param conn		Connection being received on (must be locked).
 * @param endpoint	Endpoint to receive from.
 * @param flags		Behaviour flags.
 * @param timeout	Timeout in nanoseconds.
 * @param msgp		Where to store pointer to received message.
 * @return		Status code describing result of the operation. */
static status_t
receive_message(ipc_connection_t *conn, ipc_endpoint_t *endpoint,
	unsigned flags, nstime_t timeout, ipc_kmessage_t **msgp)
{
	nstime_t absolute;
	unsigned sleep;
	ipc_kmessage_t *msg;
	status_t ret;

	assert(conn->state != IPC_CONNECTION_SETUP);
	assert(!(endpoint->flags & IPC_ENDPOINT_DROP));

	/* If the connection is closed we should still return queued messages
	 * until there is nothing left to receive, at which point we return an
	 * error. */
	if(!endpoint->message_count && conn->state == IPC_CONNECTION_CLOSED)
		return STATUS_CONN_HUNGUP;

	/* Wait for a message to arrive. */
	absolute = (timeout > 0) ? system_time() + timeout : timeout;
	sleep = SLEEP_ABSOLUTE | ((flags & IPC_INTERRUPTIBLE) ? SLEEP_INTERRUPTIBLE : 0);
	while(!endpoint->message_count) {
		ret = condvar_wait_etc(&endpoint->data_cvar, &conn->lock, absolute, sleep);

		/* Connection could have been closed while we were waiting (see
		 * ipc_connection_close()). */
		if(conn->state == IPC_CONNECTION_CLOSED)
			return STATUS_CONN_HUNGUP;

		if(ret != STATUS_SUCCESS && !endpoint->message_count)
			return ret;
	}

	assert(!list_empty(&endpoint->messages));
	msg = list_first(&endpoint->messages, ipc_kmessage_t, header);
	list_remove(&msg->header);

	if(--endpoint->message_count < IPC_QUEUE_MAX)
		condvar_signal(&endpoint->space_cvar);

	*msgp = msg;
	return STATUS_SUCCESS;
}

/**
 * Kernel interface.
 */

/**
 * Allocate a message structure.
 *
 * Allocates a new, zeroed IPC message structure. To attach data to the message,
 * use ipc_kmessage_set_data(). To attach a handle to the message, use
 * ipc_kmessage_set_handle().
 *
 * @return		Pointer to allocated message.
 */
ipc_kmessage_t *ipc_kmessage_alloc(void) {
	ipc_kmessage_t *msg;

	msg = slab_cache_alloc(ipc_kmessage_cache, MM_KERNEL);
	memset(msg, 0, sizeof(*msg));
	refcount_set(&msg->count, 1);
	list_init(&msg->header);
	return msg;
}

/** Increase the reference count of a message structure.
 * @param msg		Message to retain. */
void ipc_kmessage_retain(ipc_kmessage_t *msg) {
	refcount_inc(&msg->count);
}

/**
 * Release a message structure.
 *
 * Releases the reference count of a message structure, and frees it, along with
 * any attached data/handle, if it is no longer used.
 *
 * @param msg		Message to destroy.
 */
void ipc_kmessage_release(ipc_kmessage_t *msg) {
	if(refcount_dec(&msg->count) > 0)
		return;

	if(msg->handle)
		object_handle_release(msg->handle);

	kfree(msg->data);
	slab_cache_free(ipc_kmessage_cache, msg);
}

/**
 * Set the data attached to a message.
 *
 * Sets the data attached to a message to the specified buffer. The buffer
 * should be allocated with a kmalloc()-based function, and will become owned
 * by the message, i.e. when the message is destroyed, kfree() will be called
 * on the buffer.
 *
 * @param msg		Message to attach to.
 * @param data		Data buffer to attach (should be NULL if size is 0).
 * @param size		Size of the buffer. Must not exceed IPC_DATA_MAX.
 */
void ipc_kmessage_set_data(ipc_kmessage_t *msg, void *data, size_t size) {
	assert(!size == !data);
	assert(size <= IPC_DATA_MAX);

	if(msg->data)
		kfree(msg->data);

	msg->msg.size = size;
	msg->data = data;
}

/**
 * Set the handle attached to a message.
 *
 * Attaches the specified object handle to a message. The handle must be to a
 * transferrable object. The handle will have a new reference added to it. If
 * the message already has a handle, it will be released.
 *
 * @param msg		Message to attach to.
 * @param handle	Handle to attach (NULL to remove handle).
 */
void ipc_kmessage_set_handle(ipc_kmessage_t *msg, object_handle_t *handle) {
	assert(!handle || handle->type->flags & OBJECT_TRANSFERRABLE);

	if(msg->handle)
		object_handle_release(msg->handle);

	if(handle) {
		object_handle_retain(handle);
		msg->msg.flags |= IPC_MESSAGE_HANDLE;
	} else {
		msg->msg.flags &= ~IPC_MESSAGE_HANDLE;
	}

	msg->handle = handle;
}

/**
 * Close a connection.
 *
 * Closes a connection. The endpoint must not be used after this function has
 * returned.
 *
 * @param endpoint	Endpoint of the connection.
 */
void ipc_connection_close(ipc_endpoint_t *endpoint) {
	ipc_connection_t *conn = endpoint->conn;
	ipc_kmessage_t *msg;

	mutex_lock(&conn->lock);

	if(conn->state == IPC_CONNECTION_ACTIVE) {
		/* The connection is active so the remote process could still
		 * have threads waiting for space at this end or for messages
		 * at its end. Wake these up and they will see that the
		 * connection is now closed and return an error. */
		condvar_broadcast(&endpoint->space_cvar);
		condvar_broadcast(&endpoint->remote->data_cvar);

		/* Discard all currently queued messages. */
		LIST_FOREACH_SAFE(&endpoint->messages, iter) {
			msg = list_entry(iter, ipc_kmessage_t, header);

			list_remove(&msg->header);
			ipc_kmessage_release(msg);
		}

		endpoint->message_count = 0;

		if(endpoint->pending) {
			ipc_kmessage_release(endpoint->pending);
			endpoint->pending = NULL;
		}
	}

	if(conn->state != IPC_CONNECTION_CLOSED) {
		conn->state = IPC_CONNECTION_CLOSED;
		notifier_run(&endpoint->remote->hangup_notifier, NULL, false);
	}

	assert(notifier_empty(&endpoint->hangup_notifier));
	assert(notifier_empty(&endpoint->message_notifier));

	dprintf("ipc: closed endpoint %p (conn: %p)\n", endpoint, conn);

	mutex_unlock(&conn->lock);

	ipc_connection_release(conn);
}

/**
 * Send a message on connection.
 *
 * Queues a message at the remote end of a connection. The connection must be
 * in the active state. Messages are sent asynchronously. Message queues have a
 * finite length to prevent flooding when a process is not able to handle the
 * volume of incoming messages: if the remote message queue is full, this
 * function can block, unless the IPC_FORCE flag is set. This flag causes the
 * queue size limit to be ignored. If the IPC_INTERRUPTIBLE flag is set, the
 * thread will be interruptible while waiting for queue space.
 *
 * @param endpoint	Caller's endpoint of the connection.
 * @param msg		Message to send. Will be referenced, caller must still
 *			release it after sending.
 * @param flags		Behaviour flags.
 * @param timeout	Timeout in nanoseconds. A negative value will block
 *			until space is available in the remote message queue.
 *			A value of 0 will return an error immediately if there
 *			is no space available in the message queue. Otherwise,
 *			the function will return an error if space does not
 *			become available within the given time period.
 *
 * @return		STATUS_SUCCESS if the message was sent successfully.
 *			STATUS_WOULD_BLOCK if timeout is 0 and no space is
 *			available in the remote message queue.
 *			STATUS_TIMED_OUT if timeout passes without space
 *			becoming available in the message queue.
 *			STATUS_INTERRUPTED if the calling thread is interrupted
 *			while waiting for space in the message queue.
 *			STATUS_CONN_HUNGUP if the remote end has hung up the
 *			connection.
 */
status_t
ipc_connection_send(ipc_endpoint_t *endpoint, ipc_kmessage_t *msg,
	unsigned flags, nstime_t timeout)
{
	ipc_connection_t *conn = endpoint->conn;
	status_t ret;

	mutex_lock(&conn->lock);
	ret = queue_message(conn, endpoint->remote, msg, flags, timeout);
	mutex_unlock(&conn->lock);

	return ret;
}

/**
 * Receive a message on connection.
 *
 * Waits until a message arrives on a connection. Data or handles attached to
 * the message will be available in the returned message structure. If the
 * IPC_INTERRUPTIBLE flag is set, the calling thread will be interruptible
 * while waiting for queue space.
 *
 * @param endpoint	Endpoint of the connection.
 * @param flags		Behaviour flags.
 * @param timeout	Timeout in nanoseconds. A negative value will block
 *			until a message is received. A value of 0 will return
 *			an error immediately if there are no messages queued.
 *			Otherwise, the function will return an error if a
 *			message does not arrive within the given time period.
 * @param msgp		Where to store pointer to received message. Will be
 *			referenced, must be released when no longer needed.
 *
 * @return		STATUS_SUCCESS if a message was received.
 *			STATUS_WOULD_BLOCK if timeout is 0 and no messages are
 *			queued.
 *			STATUS_TIMED_OUT if timeout passes without a message
 *			being received.
 *			STATUS_INTERRUPTED if the calling thread is interrupted
 *			while waiting for messages.
 *			STATUS_CONN_HUNGUP if the remote end has hung up the
 *			connection.
 */
status_t
ipc_connection_receive(ipc_endpoint_t *endpoint, unsigned flags,
	nstime_t timeout, ipc_kmessage_t **msgp)
{
	ipc_connection_t *conn = endpoint->conn;
	status_t ret;

	mutex_lock(&conn->lock);
	ret = receive_message(conn, endpoint, flags, timeout, msgp);
	mutex_unlock(&conn->lock);

	return ret;
}

/**
 * Create a kernel IPC port.
 *
 * Creates a kernel IPC port. Kernel ports behave differently to normal user
 * ports: rather than requiring a thread to listen connections on them, they
 * have a callback which is called whenever a connection attempt is made on
 * them. See the documentation for ipc_port_ops_t. If the port needs to be
 * made available to userspace, use ipc_port_publish().
 *
 * @param ops		Operations structure for the port.
 * @param private	Private data pointer for use by port owner.
 *
 * @return		Pointer to created port.
 */
ipc_port_t *ipc_port_create(ipc_port_ops_t *ops, void *private) {
	ipc_port_t *port;

	assert(ops);
	assert(ops->connect);

	port = slab_cache_alloc(ipc_port_cache, MM_KERNEL);
	refcount_set(&port->count, 1);
	port->ops = ops;
	port->private = private;

	/* Owner for ports created in the kernel is kernel_proc. This is
	 * overridden by kern_port_create(). */
	port->owner = kernel_proc;
	port->owner_count = 0;

	return port;
}

/**
 * Destroy an IPC port.
 *
 * This function should be used to close a port that was created in the kernel
 * with ipc_port_create(). It disowns the port and then releases it so that it
 * can be freed once no more references to it remain.
 *
 * @param port		Port to destroy.
 */
void ipc_port_destroy(ipc_port_t *port) {
	/* This function is used to indicate that a port created in the kernel
	 * is no longer in use. Therefore we disown it, and then drop the
	 * reference added above in ipc_port_create(). */
	mutex_lock(&port->lock);
	ipc_port_disown(port);
	mutex_unlock(&port->lock);

	ipc_port_release(port);
}

/** Increase the reference count of a port.
 * @param port		Port to retain. */
void ipc_port_retain(ipc_port_t *port) {
	refcount_inc(&port->count);
}

/** Release an IPC port.
 * @param port		Port to release. */
void ipc_port_release(ipc_port_t *port) {
	if(refcount_dec(&port->count) > 0)
		return;

	assert(list_empty(&port->waiting));
	assert(notifier_empty(&port->connection_notifier));

	dprintf("ipc: destroying port %p\n", port);

	slab_cache_free(ipc_port_cache, port);
}

/**
 * Publish an IPC port to userspace.
 *
 * Creates a handle to an IPC port and publishes it in the current process'
 * handle table.
 *
 * @param port		Port to publish.
 * @param idp		If not NULL, a kernel location to store handle ID in.
 * @param uidp		If not NULL, a user location to store handle ID in.
 */
status_t ipc_port_publish(ipc_port_t *port, handle_t *idp, handle_t *uidp) {
	object_handle_t *handle;
	status_t ret;

	refcount_inc(&port->count);
	handle = object_handle_create(&port_object_type, port);
	ret = object_handle_attach(handle, idp, uidp);
	object_handle_release(handle);
	return ret;
}

/** Initialize the IPC system. */
__init_text void ipc_init(void) {
	ipc_port_cache = object_cache_create("ipc_port_cache",
		ipc_port_t, ipc_port_ctor, NULL, NULL, 0,
		MM_BOOT);
	ipc_connection_cache = object_cache_create("ipc_connection_cache",
		ipc_connection_t, ipc_connection_ctor, NULL, NULL, 0,
		MM_BOOT);
	ipc_kmessage_cache = object_cache_create("ipc_kmessage_cache",
		ipc_kmessage_t, NULL, NULL, NULL, 0,
		MM_BOOT);
}

/**
 * Userspace interface.
 */

/** Copy a message from userspace.
 * @param umsg		User message pointer.
 * @param data		Data pointer.
 * @param handle	Attached handle.
 * @param kmsgp		Where to store pointer to kernel message.
 * @return		Status code describing result of the operation. */
static status_t
copy_message_from_user(const ipc_message_t *umsg, const void *data,
	handle_t handle, ipc_kmessage_t **kmsgp)
{
	ipc_kmessage_t *kmsg = ipc_kmessage_alloc();
	status_t ret;

	if(!umsg)
		return STATUS_INVALID_ARG;

	ret = memcpy_from_user(&kmsg->msg, umsg, sizeof(kmsg->msg));
	if(ret != STATUS_SUCCESS)
		goto err;

	if(kmsg->msg.size) {
		if(kmsg->msg.size > IPC_DATA_MAX) {
			ret = STATUS_TOO_LARGE;
			goto err;
		} else if(!data) {
			ret = STATUS_INVALID_ARG;
			goto err;
		}

		kmsg->data = kmalloc(kmsg->msg.size, MM_USER);
		if(!kmsg->data) {
			ret = STATUS_NO_MEMORY;
			goto err;
		}

		ret = memcpy_from_user(kmsg->data, data, kmsg->msg.size);
		if(ret != STATUS_SUCCESS)
			goto err;
	} else if(data) {
		ret = STATUS_INVALID_ARG;
		goto err;
	}

	if(kmsg->msg.flags & IPC_MESSAGE_HANDLE) {
		ret = object_handle_lookup(handle, -1, &kmsg->handle);
		if(ret != STATUS_SUCCESS) {
			goto err;
		} else if(!(kmsg->handle->type->flags & OBJECT_TRANSFERRABLE)) {
			ret = STATUS_NOT_SUPPORTED;
			goto err;
		}
	} else if(handle >= 0) {
		ret = STATUS_INVALID_ARG;
		goto err;
	}

	*kmsgp = kmsg;
	return STATUS_SUCCESS;
err:
	ipc_kmessage_release(kmsg);
	return ret;
}

/** Connect to a port owned by a user process.
 * @param port		Port being connected to (locked).
 * @param endpoint	Endpoint for server side of the connection.
 * @param timeout	Timeout in nanoseconds.
 * @return		Status code describing result of the operation. */
static status_t
user_ipc_port_connect(ipc_port_t *port, ipc_endpoint_t *endpoint,
	nstime_t timeout)
{
	ipc_connection_t *conn = endpoint->conn;
	ipc_client_t client;
	status_t ret;

	/* Save client information. */
	client.pid = curr_proc->id;
	memcpy(&client.security, security_current_context(), sizeof(client.security));
	conn->client = &client;

	/* Queue the connection on the port. */
	list_append(&port->waiting, &conn->header);
	condvar_signal(&port->listen_cvar);
	notifier_run(&port->connection_notifier, NULL, false);

	/* We are called with the connection lock held. */
	mutex_unlock(&conn->lock);

	/* Wait for the connection to be accepted. */
	ret = condvar_wait_etc(&conn->open_cvar, &port->lock, timeout,
		SLEEP_INTERRUPTIBLE);

	mutex_lock(&conn->lock);

	if(ret != STATUS_SUCCESS) {
		/* Even if the wait failed, the connection could have been
		 * accepted while we were trying to take the locks. */
		if(conn->state == IPC_CONNECTION_ACTIVE) {
			assert(list_empty(&conn->header));
			ret = STATUS_SUCCESS;
		} else {
			/* The connection can still on the list. */
			list_remove(&conn->header);
		}
	} else {
		assert(conn->state != IPC_CONNECTION_SETUP);
	}

	/* Similarly, the connection could have been closed or the port could
	 * have been disowned (see ipc_port_disown()). */
	if(conn->state == IPC_CONNECTION_CLOSED)
		ret = STATUS_CONN_HUNGUP;

	conn->client = NULL;

	/* If we are returning success, we have to drop the reference for the
	 * server added by kern_connection_open(). kern_port_listen() adds an
	 * extra reference when it creates the connection handle so this one
	 * must be dropped. This may seem a little odd, however the reason
	 * things are done this way is so that the connect functions for
	 * kernel ports don't need to do anything with connection reference
	 * counts: if they succeed, there is a reference for their endpoint.
	 * If they fail, the reference is dropped. */
	if(ret == STATUS_SUCCESS)
		refcount_dec(&conn->count);

	return ret;
}

/** User IPC port operations. */
static ipc_port_ops_t user_ipc_port_ops = {
	.connect = user_ipc_port_connect,
};

/**
 * Create a new port.
 *
 * Creates a new IPC port. A port is a point of connection to a process. Only
 * the process that creates a port can listen for connections on the port. Any
 * process with a handle to a port is able to open a connection to it. The
 * calling process can transfer the returned handle to other processes to
 * allow them to connect to it, then listen on that handle to receive
 * connection attempts. Connections made on a port have no relation to the port
 * after they are set up: when a port's owner closes its handle to it, all
 * connections that were made on the port remain active.
 *
 * @param handlep	Where to store handle to port.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_port_create(handle_t *handlep) {
	object_handle_t *handle;
	ipc_port_t *port;
	status_t ret;

	if(!handlep)
		return STATUS_INVALID_ARG;

	port = ipc_port_create(&user_ipc_port_ops, NULL);
	port->owner = curr_proc;

	/* This handle takes over the reference added by ipc_port_create(). */
	handle = object_handle_create(&port_object_type, port);
	ret = object_handle_attach(handle, NULL, handlep);
	if(ret == STATUS_SUCCESS)
		dprintf("ipc: process %" PRId32 " created port %p\n", curr_proc->id, port);

	object_handle_release(handle);
	return ret;
}

/**
 * Listen for a connection on a port.
 *
 * Listens for a connection on the given port. Only the process that created
 * a port may listen on it. When a connection is received, a handle to the
 * server side of the connection is returned, as well as details of the client
 * process (its PID and security context).
 *
 * Once created, connection objects have no relation to the port they were
 * opened on. If the port is destroyed, any active connections remain open.
 *
 * @param handle	Handle to port to listen on.
 * @param client	Where to store client information (can be NULL).
 * @param timeout	Timeout in nanoseconds. A negative value will block
 *			until a connection attempt is received. A value of 0
 *			will return an error immediately if no connection
 *			attempts are pending. Otherwise, the function will
 *			return an error if no connection attempts are received
 *			within the given time period.
 * @param handlep	Where to store handle to server side of the connection.
 *
 * @return		STATUS_SUCCESS if a connection attempt was received.
 *			STATUS_WOULD_BLOCK if timeout is 0 and no connections
 *			are currently pending.
 *			STATUS_TIMED_OUT if timeout passes without a connection
 *			attempt.
 *			STATUS_INTERRUPTED if the calling thread is interrupted
 *			while waiting for a connection.
 */
status_t
kern_port_listen(handle_t handle, ipc_client_t *client, nstime_t timeout,
	handle_t *handlep)
{
	object_handle_t *khandle;
	ipc_port_t *port;
	nstime_t absolute;
	ipc_connection_t *conn;
	ipc_endpoint_t *endpoint;
	status_t ret;

	if(!handlep)
		return STATUS_INVALID_ARG;

	ret = object_handle_lookup(handle, OBJECT_TYPE_PORT, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	port = khandle->private;
	mutex_lock(&port->lock);

	if(curr_proc != port->owner) {
		ret = STATUS_ACCESS_DENIED;
		goto out_unlock_port;
	}

	/* Try to get a connection. We have to handle the case where the
	 * connection attempt is pulled off the list (e.g. if it times out)
	 * between getting woken and retaking the lock. */
	absolute = (timeout > 0) ? system_time() + timeout : timeout;
	while(list_empty(&port->waiting)) {
		ret = condvar_wait_etc(&port->listen_cvar, &port->lock,
			absolute, SLEEP_INTERRUPTIBLE | SLEEP_ABSOLUTE);
		if(ret != STATUS_SUCCESS && list_empty(&port->waiting))
			goto out_unlock_port;
	}

	conn = list_first(&port->waiting, ipc_connection_t, header);
	mutex_lock(&conn->lock);
	assert(conn->state == IPC_CONNECTION_SETUP);
	endpoint = &conn->endpoints[SERVER_ENDPOINT];

	if(client) {
		ret = memcpy_to_user(client, conn->client, sizeof(*client));
		if(ret != STATUS_SUCCESS)
			goto out_unlock_conn;
	}

	refcount_inc(&conn->count);
	ret = object_handle_open(&connection_object_type, endpoint, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		/* We do not want the close callback to be called if this
		 * fails, just leave the connection waiting on the port. */
		refcount_dec(&conn->count);
		goto out_unlock_conn;
	}

	/* Activate the connection and wake the connecting thread. */
	conn->state = IPC_CONNECTION_ACTIVE;
	condvar_broadcast(&conn->open_cvar);
	list_remove(&conn->header);

	dprintf("ipc: process %" PRId32 " received connection on port %p "
		"(conn: %p, endpoint: %p)\n", curr_proc->id, port, conn,
		endpoint);

	ret = STATUS_SUCCESS;

out_unlock_conn:
	mutex_unlock(&conn->lock);
out_unlock_port:
	mutex_unlock(&port->lock);
	object_handle_release(khandle);
	return ret;
}

/**
 * Open a connection on a port.
 *
 * Opens a connection to another process via a port handle, or a special port
 * identifier. The function will remain blocked until either the server receives
 * the connection, or until the given timeout expires.
 *
 * A number of per-process/per-thread special ports are defined, which can be
 * given as the port argument to this function. PROCESS_ROOT_PORT connects to
 * the current process' root port, which is typically a port owned by a service
 * manager process that can be used by processes to reach other system services.
 * THREAD_EXCEPTION_PORT connects to the current thread's exception port, which
 * is a port managed by the kernel and used to deliver notifications of thread
 * exceptions.
 *
 * @param port		Handle to port or special port ID to connect to.
 * @param timeout	Timeout in nanoseconds. A negative value will block
 *			until the connection is received. A value of 0 will
 *			return an error immediately if the connection cannot be
 *			made without delay (i.e. if the server process is not
 *			currently listening on the port). Otherwise, the
 *			function will return an error if the connection is not
 *			received within the given time period.
 * @param handlep	Where to store handle to client side of the connection.
 *
 * @return		STATUS_SUCCESS if connection is successfully opened.
 *			STATUS_CONN_HUNGUP if the port is dead.
 *			STATUS_WOULD_BLOCK if timeout is 0 and the connection
 *			cannot be accepted immediately.
 *			STATUS_TIMED_OUT if timeout passes without the
 *			connection being received.
 *			STATUS_INTERRUPTED if the calling thread is interrupted
 *			while waiting for a connection.
 */
status_t kern_connection_open(handle_t port, nstime_t timeout, handle_t *handlep) {
	object_handle_t *khandle;
	ipc_port_t *kport;
	ipc_connection_t *conn;
	ipc_endpoint_t *server, *client;
	status_t ret;

	if(!handlep)
		return STATUS_INVALID_ARG;

	if(port < 0) {
		switch(port) {
		case PROCESS_ROOT_PORT:
			kport = curr_proc->root_port;
			break;
		default:
			return STATUS_INVALID_ARG;
		}

		if(!kport)
			return STATUS_NOT_FOUND;

		refcount_inc(&kport->count);
	} else {
		ret = object_handle_lookup(port, OBJECT_TYPE_PORT, &khandle);
		if(ret != STATUS_SUCCESS)
			return ret;

		kport = khandle->private;
		refcount_inc(&kport->count);
		object_handle_release(khandle);
	}

	mutex_lock(&kport->lock);

	if(!kport->owner) {
		ret = STATUS_CONN_HUNGUP;
		goto out_unlock_port;
	}

	conn = slab_cache_alloc(ipc_connection_cache, MM_KERNEL);
	conn->state = IPC_CONNECTION_SETUP;
	server = &conn->endpoints[SERVER_ENDPOINT];
	server->flags = 0;
	client = &conn->endpoints[CLIENT_ENDPOINT];
	client->flags = 0;

	/* We initially set the reference count to 2, one for the client and
	 * one for the server. If the connect function fails, we have to drop
	 * both references (see below). */
	refcount_set(&conn->count, 2);

	mutex_lock(&conn->lock);

	ret = kport->ops->connect(kport, server, timeout);
	if(ret != STATUS_SUCCESS)
		goto err_close_conn;

	conn->state = IPC_CONNECTION_ACTIVE;
	mutex_unlock(&conn->lock);

	dprintf("ipc: process %" PRId32 " connected to port %p (conn: %p, "
		"endpoint: %p)\n", curr_proc->id, kport, conn, client);

	ret = object_handle_open(&connection_object_type, client, NULL, handlep);
	if(ret != STATUS_SUCCESS)
		ipc_connection_close(client);

	goto out_unlock_port;

err_close_conn:
	mutex_unlock(&conn->lock);
	ipc_connection_close(client);
	ipc_connection_release(conn);
out_unlock_port:
	mutex_unlock(&kport->lock);
	ipc_port_release(kport);
	return ret;
}

/**
 * Send a message on connection.
 *
 * Queues a message at the remote end of a connection. Messages are sent
 * asynchronously. Message queues have a finite length to prevent flooding when
 * a process is not able to handle the volume of incoming messages: if the
 * remote message queue is full, this function can block. If a received data
 * buffer or handle are currently pending from a previous call to
 * kern_connection_receive(), they will be discarded.
 *
 * @param handle	Handle to connection.
 * @param msg		Message to send.
 * @param data		Data to attach to message (must not be NULL if message
 *			has a non-zero size, ignored otherwise).
 * @param attached	Attached handle (must be a valid handle to a
 *			transferrable object if message has IPC_MESSAGE_HANDLE
 *			flag set, ignored otherwise).
 * @param timeout	Timeout in nanoseconds. A negative value will block
 *			until space is available in the remote message queue.
 *			A value of 0 will return an error immediately if there
 *			is no space available in the message queue. Otherwise,
 *			the function will return an error if space does not
 *			become available within the given time period.
 *
 * @return		STATUS_SUCCESS if the message was sent successfully.
 *			STATUS_WOULD_BLOCK if timeout is 0 and no space is
 *			available in the remote message queue.
 *			STATUS_TIMED_OUT if timeout passes without space
 *			becoming available in the message queue.
 *			STATUS_INTERRUPTED if the calling thread is interrupted
 *			while waiting for space in the message queue.
 *			STATUS_CONN_HUNGUP if the remote end has hung up the
 *			connection.
 */
status_t
kern_connection_send(handle_t handle, const ipc_message_t *msg, const void *data,
	handle_t attached, nstime_t timeout)
{
	object_handle_t *khandle;
	ipc_endpoint_t *endpoint;
	ipc_connection_t *conn;
	ipc_kmessage_t *kmsg;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	endpoint = khandle->private;
	conn = endpoint->conn;

	ret = copy_message_from_user(msg, data, attached, &kmsg);
	if(ret != STATUS_SUCCESS)
		goto out_release_conn;

	mutex_lock(&conn->lock);

	/* Clear any pending data left at our endpoint. */
	if(endpoint->pending) {
		ipc_kmessage_release(endpoint->pending);
		endpoint->pending = NULL;
	}

	ret = queue_message(conn, endpoint->remote, kmsg, IPC_INTERRUPTIBLE, timeout);
	mutex_unlock(&conn->lock);
	ipc_kmessage_release(kmsg);

out_release_conn:
	object_handle_release(khandle);
	return ret;
}

/**
 * Receive a message on connection.
 *
 * Waits until a message arrives on a connection and copies it into the
 * supplied buffer. If the message has a data buffer attached, indicated by a
 * non-zero size in the returned message, it can be retrieved by calling
 * kern_connection_receive_data(). If it has a handle attached, indicated by
 * the IPC_MESSAGE_HANDLE flag in the returned message, it can be retrieved by
 * calling kern_connection_receive_handle(). Any attached data will be available
 * until the next call to kern_connection_send() or kern_connection_receive()
 * on the connection, at which point data that has not been retrieved will be
 * dropped.
 *
 * @param handle	Handle to connection.
 * @param msg		Where to store received message.
 * @param security	Where to store the security context of the thread that
 *			sent the message at the time the message was sent (can
 *			be NULL).
 * @param timeout	Timeout in nanoseconds. A negative value will block
 *			until a message is received. A value of 0 will return
 *			an error immediately if there are no messages queued.
 *			Otherwise, the function will return an error if a
 *			message does not arrive within the given time period.
 *
 * @return		STATUS_SUCCESS if a message was received.
 *			STATUS_WOULD_BLOCK if timeout is 0 and no messages are
 *			queued.
 *			STATUS_TIMED_OUT if timeout passes without a message
 *			being received.
 *			STATUS_INTERRUPTED if the calling thread is interrupted
 *			while waiting for messages.
 *			STATUS_CONN_HUNGUP if the remote end has hung up the
 *			connection.
 */
status_t
kern_connection_receive(handle_t handle, ipc_message_t *msg,
	security_context_t *security, nstime_t timeout)
{
	object_handle_t *khandle;
	ipc_endpoint_t *endpoint;
	ipc_connection_t *conn;
	ipc_kmessage_t *kmsg;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	endpoint = khandle->private;
	conn = endpoint->conn;

	mutex_lock(&conn->lock);

	/* Clear any pending data left at our endpoint. */
	if(endpoint->pending) {
		ipc_kmessage_release(endpoint->pending);
		endpoint->pending = NULL;
	}

	ret = receive_message(conn, endpoint, IPC_INTERRUPTIBLE, timeout, &kmsg);
	if(ret != STATUS_SUCCESS)
		goto out_unlock_conn;

	ret = memcpy_to_user(msg, &kmsg->msg, sizeof(*msg));
	if(ret != STATUS_SUCCESS) {
		/* The message is lost in this case, but they shouldn't have
		 * given us a bad pointer... */
		ipc_kmessage_release(kmsg);
		goto out_unlock_conn;
	}

	if(security) {
		ret = memcpy_to_user(security, &kmsg->security, sizeof(*security));
		if(ret != STATUS_SUCCESS) {
			/* Same as above. */
			ipc_kmessage_release(kmsg);
			goto out_unlock_conn;
		}
	}

	/* Save the message if there is data or a handle to retrieve, otherwise
	 * free it. */
	if(ipc_kmessage_has_attachment(kmsg)) {
		/* Hmm, not sure whether this is actually necessary. */
		if(endpoint->pending)
			ipc_kmessage_release(endpoint->pending);
		endpoint->pending = kmsg;
	} else {
		ipc_kmessage_release(kmsg);
	}

out_unlock_conn:
	mutex_unlock(&conn->lock);
	object_handle_release(khandle);
	return ret;
}

/**
 * Receive data attached to a message.
 *
 * Copies the data attached to the last received message on a connection to
 * a buffer. Upon successful completion, the stored copy of the data will be
 * dropped and will not be available again by a subsequent call to this
 * function.
 *
 * @param handle	Handle to connection.
 * @param data		Buffer to copy into. This should be at least the size
 *			indicated in the received message. If NULL, the
 *			pending data will be dropped without being copied.
 *
 * @return		STATUS_SUCCESS if data is copied successfully.
 *			STATUS_NOT_FOUND if no pending data is available.
 */
status_t kern_connection_receive_data(handle_t handle, void *data) {
	object_handle_t *khandle;
	ipc_endpoint_t *endpoint;
	ipc_kmessage_t *msg;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	endpoint = khandle->private;

	mutex_lock(&endpoint->conn->lock);

	msg = endpoint->pending;
	if(msg && msg->data) {
		/* Just drop the data if the pointer is NULL. */
		ret = (data)
			? memcpy_to_user(data, msg->data, msg->msg.size)
			: STATUS_SUCCESS;
		if(ret == STATUS_SUCCESS) {
			ipc_kmessage_set_data(msg, NULL, 0);

			/* Discard if now empty. */
			if(!msg->handle) {
				ipc_kmessage_release(msg);
				endpoint->pending = NULL;
			}
		}
	} else {
		ret = STATUS_NOT_FOUND;
	}

	mutex_unlock(&endpoint->conn->lock);
	object_handle_release(khandle);
	return ret;
}

/**
 * Receive a handle attached to a message.
 *
 * Receives the handle attached to the last received message on a connection.
 * Upon successful completion, the stored handle will be dropped and will not
 * be available again by a subsequent call to this function.
 *
 * @param handle	Handle to connection.
 * @param attachedp	Where to store handle received. If NULL, the pending
 *			handle will be dropped.
 *
 * @return		STATUS_SUCCESS if data is copied successfully.
 *			STATUS_NOT_FOUND if no pending data is available.
 *			STATUS_NO_HANDLES if the calling process' handle table
 *			is full.
 */
status_t kern_connection_receive_handle(handle_t handle, handle_t *attachedp) {
	object_handle_t *khandle;
	ipc_endpoint_t *endpoint;
	ipc_kmessage_t *msg;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	endpoint = khandle->private;

	mutex_lock(&endpoint->conn->lock);

	msg = endpoint->pending;
	if(msg && msg->handle) {
		/* Just drop the handle if the pointer is NULL. */
		ret = (attachedp)
			? object_handle_attach(msg->handle, NULL, attachedp)
			: STATUS_SUCCESS;
		if(ret == STATUS_SUCCESS) {
			ipc_kmessage_set_handle(msg, NULL);

			/* Discard if now empty. */
			if(!msg->data) {
				ipc_kmessage_release(msg);
				endpoint->pending = NULL;
			}
		}
	} else {
		ret = STATUS_NOT_FOUND;
	}

	mutex_unlock(&endpoint->conn->lock);
	object_handle_release(khandle);
	return ret;
}
