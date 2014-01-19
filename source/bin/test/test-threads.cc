/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Test application.
 */

#include <kernel/status.h>
#include <kernel/thread.h>

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#define NUM_THREADS	8

static std::mutex test_lock;
static std::condition_variable test_cond;
static bool exiting = false;

static void thread_func(void *id) {
	std::unique_lock<std::mutex> lock(test_lock);

	if((unsigned long)id == 0) {
		while(!exiting) {
			lock.unlock();
			sleep(1);
			lock.lock();

			printf("Broadcasting\n");
			test_cond.notify_all();
		}
	} else {
		while(!exiting) {
			printf("Thread %u waiting\n", (unsigned long)id);
			test_cond.wait(lock);
			printf("Thread %u woken\n", (unsigned long)id);
		}
	}
}

int main(int argc, char **argv) {
	status_t ret;

	std::cout << "Hello, World! My arguments are:" << std::endl;

	std::vector<std::string> args;
	args.assign(argv, argv + argc);

	int i = 0;
	for(const std::string &arg : args)
		std::cout << " args[" << i++ << "] = '" << arg << "'" << std::endl;

	printf("Acquiring lock...\n");
	test_lock.lock();

	printf("Creating threads...\n");

	object_event_t events[NUM_THREADS];
	for(i = 0; i < NUM_THREADS; i++) {
		thread_entry_t entry;

		entry.func = thread_func;
		entry.arg = (void *)(unsigned long)i;
		entry.stack = NULL;
		entry.stack_size = 0;

		ret = kern_thread_create("test", &entry, 0, &events[i].handle);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to create thread: %d\n", ret);
			return EXIT_FAILURE;
		}

		events[i].event = THREAD_EVENT_DEATH;

		printf("Created thread %" PRId32 ", handle %" PRId32 "\n",
			kern_thread_id(events[i].handle), events[i].handle);
	}

	printf("Unlocking...\n");
	test_lock.unlock();

	sleep(20);

	test_lock.lock();
	printf("Exiting...\n");
	exiting = true;
	test_lock.unlock();

	ret = kern_object_wait(events, NUM_THREADS, OBJECT_WAIT_ALL, -1);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to wait for thread: %d\n", ret);
		return EXIT_FAILURE;
	}

	printf("All threads exited\n");
	return 0;
}
