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
 * @brief 	Timer class
 */

#include <kernel/time.h>

#include <Error.h>
#include <Timer.h>

#include <cassert>

#include "Internal.h"

using namespace pulsar;

/**
 * Constructor for Timer
 *
 * @param mode 	Mode for timer. If OneShotMode, the timer will
 * 			only fire once after it is started. If PeriodicMode,
 *			it will fire periodically after being started, until it
 * 			is stopped with Stop()
 * @throw 	If unable to create the timer. This can only happen if
 			the process handle table is full.
 */
Timer::Timer(Mode mode) : m_mode(mode), m_running(false)
{
	handle_t handle;
	status_t ret;

	assert(mode == OneShotMode || mode == PeriodicMode);

	ret = kern_timer_create(0, &handle);
	if (unlikely(ret != STATUS_SUCCESS))
	{
		throw Error(ret);
	}

	SetHandle(handle);
}

/**
 * Start the timer
 * 
 * @param internal 	Internal for the timer
 */
void Timer::Start(useconds_t interval)
{
	status_t ret;
	int mode;

	assert(interval > 0);

	mode = (m_mode == PeriodicMode) ? TIMER_PERIODIC : TIMER_ONESHOT;
	ret = kern_timer_start(m_handle, interval, mode);
	assert(ret == STATUS_SUCCESS);
	m_running = true;
}

/**
 * Stop the timer
 */
void Timer::Stop()
{
	status_t ret = kern_timer_stop(m_handle, 0);
	assert(ret == STATUS_SUCCESS);
	m_running = false;
}

/**
 * Register events with the event loop
 */
void Timer::RegisterEvents()
{
	RegisterEvent(TIMER_EVENT);
}

/**
 * Handle an event on the timer
 *
 * @param event Event ID
 */
void Timer::HandleEvent(int event)
{
	assert(event == TIMER_EVENT);

	if (m_mode == OneShotMode)
	{
		m_running = false;
	}

	OnTimer();
}
