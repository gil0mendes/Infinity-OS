/*
 * Copyright (C) 2014 Gil Mendes
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
 * @brief 	Thread class
 */

#include <kernel/object.h>
#include <kernel/thread.h>

#include <Thread.h>

#include <cassert>
#include <string>

#include "Internal.h"

using namespace pulsar;

/** 
 * Internal data for Thread.
 */
struct pulsar::ThreadPrivate 
{
	ThreadPrivate() : name("user_thread"), event_loop(0) {}

	std::string name;			// Name to give the thread
	EventLoop *event_loop;		// Event loop for the thread
};

/** 
 * Set up the thread object.
 * 
 * @note	The thread is not created here. Once the object has
 *			been initialised you can start a new thread using
 *			Run().
 *
 * @param handle 	If not negative, a existing thread handle to make the
 *					object use. Must refer to a thread object. 
 */
Thread::Thread(handle_t handle) : m_priv(new ThreadPrivate)
{
	if(handle >= 0) 
	{
		if(unlikely(kern_object_type(handle) != OBJECT_TYPE_THREAD)) 
		{
			libpulsar_fatal("Thread::Thread: Handle must refer to a thread object.");
		}

		SetHandle(handle);
	}

	m_priv->event_loop = new EventLoop(true);
}

/** 
 * Destroy the thread object.
 *
 * Destroys the thread object. It should not be running. If any handles are
 * still attached to the thread's event loop, they will be moved to the calling
 * thread's event loop.
 */
Thread::~Thread() 
{
	assert(!IsRunning());

	if(m_priv->event_loop) 
	{
		EventLoop *current = EventLoop::Instance();
		if(current) 
		{
			// Move handles from the thread's event loop to the
			// current thread's
			current->Merge(m_priv->event_loop);
		} 
		else 
		{
			// Throw up a warning: when we destroy the loop the
			// handles it contains will have an invalid event loop
			// pointer. FIXME: Only do this if there are handles in
			// the loop
			libpulsar_warn("Thread::~Thread: No event loop to move handles to.");
		}

		delete m_priv->event_loop;
	}

	delete m_priv;
}

/** 
 * Set the name to use for a new thread.
 * 
 * @param name		Name to use for the thread. 
 */
void Thread::SetName(const char *name) 
{
	m_priv->name = name;
}

/** 
 * Start the thread.
 * 
 * @return	True if succeeded in creating thread, false if not.
 *			More information about an error can be retrieved by
 *			calling GetError(). 
 */
bool Thread::Run() 
{
	handle_t handle;
	status_t ret;

	thread_entry_t entry;
	entry.func = &Thread::_Entry; 	// Function
	entry.arg = 0; 					// Argument
	entry.stack = NULL; 			// Stack
	entry.stack_size = 0; 			// Stack Size

	ret = kern_thread_create(m_priv->name.c_str(), &entry, 0, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) 
	{
		SetError(ret);
		return false;
	}

	SetHandle(handle);
	return true;
}

/** 
 * Wait for the thread to exit.
 * 
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the thread has not already exited,
 *			and a value of -1 will block indefinitely until the
 *			thread exits.
 * 
 * @return		True if thread exited within the timeout, false if not. 
 */
bool Thread::Wait(useconds_t timeout) const 
{
	return (_Wait(THREAD_EVENT_DEATH, timeout) == STATUS_SUCCESS);
}

/** 
 * Ask the thread to quit.
 * 
 * @param status	Status to make the thread's event loop return with. 
 */
void Thread::Quit(int status) 
{
	if(IsRunning()) 
	{
		m_priv->event_loop->Quit(status);
	}
}

/** 
 * Check whether the thread is running.
 * 
 * @return		Whether the thread is running. 
 */
bool Thread::IsRunning() const 
{
	int status;
	return (m_handle >= 0 && kern_thread_status(m_handle, &status) == STATUS_STILL_RUNNING);
}

/** 
 * Get the exit status of the thread.
 * 
 * @return		Exit status of the thread, or -1 if still running. 
 */
int Thread::GetStatus() const 
{
	int status;

	if(kern_thread_status(m_handle, &status) != STATUS_SUCCESS) 
	{
		return -1;
	}
	return status;
}

/** 
 * Get the ID of the thread.
 * 
 * @return		ID of the thread. 
 */
thread_id_t Thread::GetID() const 
{
	return kern_thread_id(m_handle);
}

/** 
 * Get the ID of the current thread.
 * 
 * @return		ID of the current thread. 
 */
thread_id_t Thread::GetCurrentID() 
{
	return kern_thread_id(-1);
}

/** 
 * Sleep for a certain time period.
 * 
 * @param usecs		Microseconds to sleep for. 
 */
void Thread::Sleep(useconds_t usecs) 
{
	kern_thread_sleep(usecs, NULL);
}

/** 
 * Get the thread's event loop.
 * 
 * @return		Reference to thread's event loop. 
 */
EventLoop &Thread::GetEventLoop() 
{
	return *m_priv->event_loop;
}

/** 
 * Main function for the thread.
 *
 * The main function for the thread, which is called when the thread starts
 * running. This can be overridden by derived classes to do their own work.
 * The default implementation just runs the thread's event loop.
 *
 * @return		Exit status code for the thread.
 */
int Thread::Main() 
{
	return m_priv->event_loop->Run();
}

/** 
 * Register events with the event loop. 
 */
void Thread::RegisterEvents() 
{
	RegisterEvent(THREAD_EVENT_DEATH);
}

/** 
 * Handle an event from the thread.
 * 
 * @param event		Event ID. 
 */
void Thread::HandleEvent(int event) {
	if(event == THREAD_EVENT_DEATH) {
		int status = 0;
		kern_thread_status(m_handle, &status);
		OnExit(status);

		/* Unregister the death event so that it doesn't continually
		 * get signalled. */
		UnregisterEvent(THREAD_EVENT_DEATH);
	}
}

/** 
 * Entry point for a new thread.
 * 
 * @param arg		Pointer to Thread object. 
 */
void Thread::_Entry(void *arg) {
	Thread *thread = reinterpret_cast<Thread *>(arg);

	/* Set the event loop pointer. */
	g_event_loop = thread->m_priv->event_loop;

	/* Call the main function. */
	kern_thread_exit(thread->Main());
}