/*
 * Copyright (C) 2010-2013 Gil Mendes
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
 * @brief		Time handling functions.
 *
 * @todo		Timers are tied to the CPU that they are created on.
 *			This is the right thing to do with, e.g. the scheduler
 *			timers, but what should we do with user timers? Load
 *			balance them? They'll probably get balanced reasonably
 *			due to thread load balancing. Does it matter that much?
 */

#include <kernel/time.h>

#include <lib/notifier.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/thread.h>

#include <assert.h>
#include <cpu.h>
#include <dpc.h>
#include <kdb.h>
#include <kernel.h>
#include <object.h>
#include <status.h>
#include <time.h>

/** Userspace timer structure. */
typedef struct user_timer {
	uint32_t flags;			/**< Flags for the timer. */
	timer_t timer;			/**< Kernel timer. */
	notifier_t notifier;		/**< Notifier for the timer event. */
	bool fired;			/**< Whether the event has fired. */
} user_timer_t;

/** Check if a year is a leap year. */
#define LEAPYR(y)	(((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

/** Get number of days in a year. */
#define DAYS(y)		(LEAPYR(y) ? 366 : 365)

/** Table containing number of days before a month. */
static unsigned days_before_month[] = {
	0,
	/* Jan. */ 0,
	/* Feb. */ 31,
	/* Mar. */ 31 + 28,
	/* Apr. */ 31 + 28 + 31,
	/* May. */ 31 + 28 + 31 + 30,
	/* Jun. */ 31 + 28 + 31 + 30 + 31,
	/* Jul. */ 31 + 28 + 31 + 30 + 31 + 30,
	/* Aug. */ 31 + 28 + 31 + 30 + 31 + 30 + 31,
	/* Sep. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
	/* Oct. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
	/* Nov. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
	/* Dec. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
};

/** The number of nanoseconds since the Epoch the kernel was booted at. */
nstime_t boot_unix_time = 0;

/** Hardware timer device. */
static timer_device_t *timer_device = NULL;

/** Convert a date/time to nanoseconds since the epoch.
 * @param year		Year.
 * @param month		Month (1-12).
 * @param day		Day of month (1-31).
 * @param hour		Hour (0-23).
 * @param min		Minute (0-59).
 * @param sec		Second (0-59).
 * @return		Number of nanoseconds since the epoch. */
nstime_t
time_to_unix(unsigned year, unsigned month, unsigned day, unsigned hour,
	unsigned min, unsigned sec)
{
	uint32_t seconds = 0;
	unsigned i;

	/* Start by adding the time of day and day of month together. */
	seconds += sec;
	seconds += min * 60;
	seconds += hour * 60 * 60;
	seconds += (day - 1) * 24 * 60 * 60;

	/* Convert the month into days. */
	seconds += days_before_month[month] * 24 * 60 * 60;

	/* If this year is a leap year, and we're past February, we need to
	 * add another day. */
	if(month > 2 && LEAPYR(year))
		seconds += 24 * 60 * 60;

	/* Add the days in each year before this year from 1970. */
	for(i = 1970; i < year; i++)
		seconds += DAYS(i) * 24 * 60 * 60;

	return SECS2NSECS(seconds);
}

/**
 * Get the number of nanoseconds since the Unix Epoch.
 *
 * Returns the number of nanoseconds that have passed since the Unix Epoch,
 * 00:00:00 UTC, January 1st, 1970.
 *
 * @return		Number of nanoseconds since epoch.
 */
nstime_t unix_time(void) {
	return boot_unix_time + system_time();
}

/** Get the system boot time.
 * @return		UNIX time at which the system was booted. */
nstime_t boot_time(void) {
	return boot_unix_time;
}

/** Prepares next timer tick.
 * @param timer		Timer to prepare for. */
static void timer_device_prepare(timer_t *timer) {
	nstime_t length = timer->target - system_time();
	timer_device->prepare((length > 0) ? length : 1);
}

/** Ensure that the timer device is enabled. */
static inline void timer_device_enable(void) {
	/* The device may not be disabled when we expect it to be (if
	 * timer_stop() is run from a different CPU to the one the timer is
	 * running on, it won't be able to disable the timer if the list
	 * becomes empty). */
	if(!curr_cpu->timer_enabled) {
		timer_device->enable();
		curr_cpu->timer_enabled = true;
	}
}

/** Disable the timer device. */
static inline void timer_device_disable(void) {
	/* The timer device should always be enabled when we expect it to be. */
	assert(curr_cpu->timer_enabled);

	timer_device->disable();
	curr_cpu->timer_enabled = false;
}

/**
 * Set the timer device.
 *
 * Sets the device that will provide timer ticks. This function must only be
 * called once.
 *
 * @param device	Device to set.
 */
void timer_device_set(timer_device_t *device) {
	assert(!timer_device);

	timer_device = device;
	if(timer_device->type == TIMER_DEVICE_ONESHOT)
		curr_cpu->timer_enabled = true;

	kprintf(LOG_NOTICE, "timer: activated timer device %s\n", device->name);
}

/** Start a timer, with CPU timer lock held.
 * @param timer		Timer to start. */
static void timer_start_unsafe(timer_t *timer) {
	timer_t *exist;
	list_t *pos;

	assert(list_empty(&timer->header));

	/* Work out the absolute completion time. */
	timer->target = system_time() + timer->initial;

	/* Find the insertion point for the timer: the list must be ordered
	 * with nearest expiration time first. */
	pos = curr_cpu->timers.next;
	while(pos != &curr_cpu->timers) {
		exist = list_entry(pos, timer_t, header);
		if(exist->target > timer->target)
			break;

		pos = pos->next;
	}

	list_add_before(pos, &timer->header);
}

/** DPC function to run a timer function.
 * @param _timer	Pointer to timer. */
static void timer_dpc_request(void *_timer) {
	timer_t *timer = _timer;
	timer->func(timer->data);
}

/** Handles a timer tick.
 * @return		Whether to preempt the current thread. */
bool timer_tick(void) {
	nstime_t time = system_time();
	bool preempt = false;
	timer_t *timer;

	assert(timer_device);
	assert(!local_irq_state());

	if(!curr_cpu->timer_enabled)
		return false;

	spinlock_lock(&curr_cpu->timer_lock);

	/* Iterate the list and check for expired timers. */
	LIST_FOREACH_SAFE(&curr_cpu->timers, iter) {
		timer = list_entry(iter, timer_t, header);

		/* Since the list is ordered soonest expiry first, we can break
		 * if the current timer has not expired. */
		if(time < timer->target)
			break;

		/* This timer has expired, remove it from the list. */
		list_remove(&timer->header);

		/* Perform its timeout action. */
		if(timer->flags & TIMER_THREAD) {
			dpc_request(timer_dpc_request, timer);
		} else {
			if(timer->func(timer->data))
				preempt = true;
		}

		/* If the timer is periodic, restart it. */
		if(timer->mode == TIMER_PERIODIC)
			timer_start_unsafe(timer);
	}

	switch(timer_device->type) {
	case TIMER_DEVICE_ONESHOT:
		/* Prepare the next tick if there is still a timer in the list. */
		if(!list_empty(&curr_cpu->timers))
			timer_device_prepare(list_first(&curr_cpu->timers, timer_t, header));

		break;
	case TIMER_DEVICE_PERIODIC:
		/* For periodic devices, if the list is empty disable the
		 * device so the timer does not interrupt unnecessarily. */
		if(list_empty(&curr_cpu->timers))
			timer_device_disable();

		break;
	}

	spinlock_unlock(&curr_cpu->timer_lock);
	return preempt;
}

/** Initialize a timer structure.
 * @param timer		Timer to initialize.
 * @param name		Name of the timer for debugging purposes.
 * @param func		Function to call when the timer expires.
 * @param data		Data argument to pass to timer.
 * @param flags		Behaviour flags for the timer. */
void timer_init(timer_t *timer, const char *name, timer_func_t func, void *data, uint32_t flags) {
	list_init(&timer->header);
	timer->func = func;
	timer->data = data;
	timer->flags = flags;
	timer->name = name;
}

/** Start a timer.
 * @param timer		Timer to start. Must not already be running.
 * @param length	Nanoseconds to run the timer for. If 0 or negative
 *			the function will do nothing.
 * @param mode		Mode for the timer. */
void timer_start(timer_t *timer, nstime_t length, unsigned mode) {
	bool state;

	if(length <= 0)
		return;

	/* Prevent curr_cpu from changing underneath us. */
	state = local_irq_disable();

	timer->cpu = curr_cpu;
	timer->mode = mode;
	timer->initial = length;

	spinlock_lock_noirq(&curr_cpu->timer_lock);

	/* Add the timer to the list. */
	timer_start_unsafe(timer);

	switch(timer_device->type) {
	case TIMER_DEVICE_ONESHOT:
		/* If the new timer is at the beginning of the list, then it
		 * has the shortest remaining time, so we need to adjust the
		 * device to tick for it. */
		if(timer == list_first(&curr_cpu->timers, timer_t, header))
			timer_device_prepare(timer);

		break;
	case TIMER_DEVICE_PERIODIC:
		/* Enable the device. */
		timer_device_enable();
		break;
	}

	spinlock_unlock_noirq(&curr_cpu->timer_lock);
	local_irq_restore(state);
}

/** Cancel a running timer.
 * @param timer		Timer to stop. */
void timer_stop(timer_t *timer) {
	timer_t *first;

	if(!list_empty(&timer->header)) {
		assert(timer->cpu);

		spinlock_lock(&timer->cpu->timer_lock);

		first = list_first(&timer->cpu->timers, timer_t, header);
		list_remove(&timer->header);

		/* If the timer is running on this CPU, adjust the tick length
		 * or disable the device if required. If the timer is on another
		 * CPU, it's no big deal: the tick handler is able to handle
		 * unexpected ticks. */
		if(timer->cpu == curr_cpu) {
			switch(timer_device->type) {
			case TIMER_DEVICE_ONESHOT:
				if(first == timer && !list_empty(&curr_cpu->timers)) {
					first = list_first(&curr_cpu->timers, timer_t, header);
					timer_device_prepare(first);
				}
				break;
			case TIMER_DEVICE_PERIODIC:
				if(list_empty(&curr_cpu->timers))
					timer_device_disable();

				break;
			}
		}

		spinlock_unlock(&timer->cpu->timer_lock);
	}
}

/** Sleep for a certain amount of time.
 * @param nsecs		Nanoseconds to sleep for (must be greater than or
 *			equal to 0). If SLEEP_ABSOLUTE is specified, this is
 *			a target system time to sleep until.
 * @param flags		Behaviour flags (see sync/sync.h).
 * @return		STATUS_SUCCESS on success, STATUS_INTERRUPTED if
 *			SLEEP_INTERRUPTIBLE specified and sleep was interrupted. */
status_t delay_etc(nstime_t nsecs, int flags) {
	status_t ret;

	assert(nsecs >= 0);

	ret = thread_sleep(NULL, nsecs, "usleep", flags);
	if(likely(ret == STATUS_TIMED_OUT || ret == STATUS_WOULD_BLOCK))
		ret = STATUS_SUCCESS;

	return ret;
}

/** Delay for a period of time.
 * @param nsecs		Nanoseconds to sleep for (must be greater than or
 *			equal to 0). */
void delay(nstime_t nsecs) {
	delay_etc(nsecs, 0);
}

/** Dump a list of timers.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_timers(int argc, char **argv, kdb_filter_t *filter) {
	timer_t *timer;
	uint64_t id;
	cpu_t *cpu;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<CPU ID>]\n\n", argv[0]);

		kdb_printf("Prints a list of all timers on a CPU. If no ID given, current CPU\n");
		kdb_printf("will be used.\n");
		return KDB_SUCCESS;
	} else if(argc != 1 && argc != 2) {
		kdb_printf("Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(argc == 2) {
		if(kdb_parse_expression(argv[1], &id, NULL) != KDB_SUCCESS)
			return KDB_FAILURE;

		if(id > highest_cpu_id || !(cpu = cpus[id])) {
			kdb_printf("Invalid CPU ID.\n");
			return KDB_FAILURE;
		}
	} else {
		cpu = curr_cpu;
	}

	kdb_printf("Name                 Target           Function           Data\n");
	kdb_printf("====                 ======           ========           ====\n");

	LIST_FOREACH(&cpu->timers, iter) {
		timer = list_entry(iter, timer_t, header);

		kdb_printf("%-20s %-16llu %-18p %p\n", timer->name, timer->target,
			timer->func, timer->data);
	}

	return KDB_SUCCESS;
}

/** Print the system uptime.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @param filter	Unused.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_uptime(int argc, char **argv, kdb_filter_t *filter) {
	nstime_t time;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Prints how much time has passed since the kernel started.\n");
		return KDB_SUCCESS;
	}

	time = system_time();
	kdb_printf("%llu seconds (%llu nanoseconds)\n", NSECS2SECS(time), time);
	return KDB_SUCCESS;
}

/** Initialize the timing system. */
__init_text void time_init(void) {
	/* Initialize the boot time. */
	boot_unix_time = platform_time_from_hardware() - system_time();

	/* Register debugger commands. */
	kdb_register_command("timers", "Print a list of running timers.", kdb_cmd_timers);
	kdb_register_command("uptime", "Display the system uptime.", kdb_cmd_uptime);
}

/** Closes a handle to a timer.
 * @param handle	Handle being closed. */
static void timer_object_close(object_handle_t *handle) {
	user_timer_t *timer = handle->private;

	notifier_clear(&timer->notifier);
	kfree(timer);
}

/** Signal that a timer is being waited for.
 * @param handle	Handle to timer.
 * @param event		Event to wait for.
 * @param wait		Internal wait data pointer.
 * @return		Status code describing result of the operation. */
static status_t timer_object_wait(object_handle_t *handle, unsigned event, void *wait) {
	user_timer_t *timer = handle->private;

	switch(event) {
	case TIMER_EVENT_FIRED:
		if(timer->fired) {
			timer->fired = false;
			object_wait_signal(wait, 0);
		} else {
			notifier_register(&timer->notifier, object_wait_notifier, wait);
		}

		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a timer.
 * @param handle	Handle to timer.
 * @param event		Event to wait for.
 * @param wait		Internal wait data pointer. */
static void timer_object_unwait(object_handle_t *handle, unsigned event, void *wait) {
	user_timer_t *timer = handle->private;

	switch(event) {
	case TIMER_EVENT_FIRED:
		notifier_unregister(&timer->notifier, object_wait_notifier, wait);
		break;
	}
}

/** Timer object type. */
static object_type_t timer_object_type = {
	.id = OBJECT_TYPE_TIMER,
	.flags = OBJECT_TRANSFERRABLE,
	.close = timer_object_close,
	.wait = timer_object_wait,
	.unwait = timer_object_unwait,
};

/** Timer handler function for a userspace timer.
 * @param _timer	Pointer to timer.
 * @return		Whether to preempt. */
static bool user_timer_func(void *_timer) {
	user_timer_t *timer = _timer;

	/* Signal the event. */
	if(!notifier_run(&timer->notifier, NULL, true))
		timer->fired = true;

	return false;
}

/** Create a new timer.
 * @param flags		Flags for the timer.
 * @param handlep	Where to store handle to timer object.
 * @return		Status code describing result of the operation. */
status_t kern_timer_create(uint32_t flags, handle_t *handlep) {
	user_timer_t *timer;
	object_handle_t *handle;
	status_t ret;

	if(!handlep)
		return STATUS_INVALID_ARG;

	timer = kmalloc(sizeof(*timer), MM_KERNEL);
	timer_init(&timer->timer, "timer_object", user_timer_func, timer, TIMER_THREAD);
	notifier_init(&timer->notifier, timer);
	timer->flags = flags;
	timer->fired = false;

	handle = object_handle_create(&timer_object_type, timer);
	ret = object_handle_attach(handle, NULL, handlep);
	object_handle_release(handle);
	return ret;
}

/** Start a timer.
 * @param handle	Handle to timer object.
 * @param interval	Interval of the timer in nanoseconds.
 * @param mode		Mode of the timer. If TIMER_ONESHOT, the timer event
 *			will only be fired once after the specified time period.
 *			If TIMER_PERIODIC, it will be fired periodically at the
 *			specified interval, until timer_stop() is called.
 * @return		Status code describing result of the operation. */
status_t kern_timer_start(handle_t handle, nstime_t interval, unsigned mode) {
	object_handle_t *khandle;
	user_timer_t *timer;
	status_t ret;

	if(interval <= 0 || (mode != TIMER_ONESHOT && mode != TIMER_PERIODIC))
		return STATUS_INVALID_ARG;

	ret = object_handle_lookup(handle, OBJECT_TYPE_TIMER, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	timer = khandle->private;

	timer_stop(&timer->timer);
	timer_start(&timer->timer, interval, mode);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
}

/** Stop a timer.
 * @param handle	Handle to timer object.
 * @param remp		If not NULL, where to store remaining time.
 * @return		Status code describing result of the operation. */
status_t kern_timer_stop(handle_t handle, nstime_t *remp) {
	object_handle_t *khandle;
	user_timer_t *timer;
	nstime_t rem;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_TIMER, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	timer = khandle->private;

	if(!list_empty(&timer->timer.header)) {
		timer_stop(&timer->timer);
		if(remp) {
			rem = system_time() - timer->timer.target;
			ret = write_user(remp, rem);
		}
	} else if(remp) {
		ret = memset_user(remp, 0, sizeof(*remp));
	}

	object_handle_release(khandle);
	return ret;
}

/**
 * Get the current time from a time source.
 *
 * Gets the current time, in nanoseconds, from the specified time source. There
 * are currently 2 time sources defined:
 *  - TIME_SYSTEM: A monotonic timer which gives the time since the system was
 *    started.
 *  - TIME_REAL: Real time given as time since the UNIX epoch. This can be
 *    changed with kern_time_set().
 *
 * @param source	Time source to get from.
 * @param timep		Where to store time in nanoseconds.
 *
 * @return		STATUS_SUCCESS on success.
 *			STATUS_INVALID_ARG if time source is invalid or timep is
 *			NULL.
 */
status_t kern_time_get(unsigned source, nstime_t *timep) {
	nstime_t time;

	if(!timep)
		return STATUS_INVALID_ARG;

	switch(source) {
	case TIME_SYSTEM:
		time = system_time();
		break;
	case TIME_REAL:
		time = unix_time();
		break;
	default:
		return STATUS_INVALID_ARG;
	}

	return write_user(timep, time);
}

/**
 * Set the current time.
 *
 * Sets the current time, in nanoseconds, for a time source. Currently only
 * the TIME_REAL source (see kern_time_get()) can be changed.
 *
 * @param source	Time source to set.
 * @param time		New time value in nanoseconds.
 *
 * @return		STATUS_SUCCESS on success.
 *			STATUS_INVALID_ARG if time source is invalid.
 */
status_t kern_time_set(unsigned source, nstime_t time) {
	return STATUS_NOT_IMPLEMENTED;
}
