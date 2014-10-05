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
 * @brief 	Shutdown command
 */

#include <kernel/system.h>
#include <kernel/status.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * Main function for the shutdown command
 */
int main(int argc, char **argv)
{
	int action = SHUTDOWN_POWEROFF;
	status_t ret;

	// Check if have arguments
	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0)
		{
			printf("Usage: %s [-r]\n", argv[0]);
			return EXIT_SUCCESS;
		}
		else if(strcmp(argv[1], "-r") == 0)
		{
			action = SHUTDOWN_REBOOT;
		}
	}

	// If binari name is "reboot"
	if (strcmp(argv[0], "reboot") == 0)
	{
		action = SHUTDOWN_REBOOT;
	}

	ret = kern_system_shutdown(action);
	printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
	return EXIT_SUCCESS;
}
