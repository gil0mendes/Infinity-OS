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

#ifndef __PULSAR_THREAD_H
#define __PULSAR_THREAD_H

#include <Error.h>
#include <EventLoop.h>
#include <Handle.h>

namespace pulsar
{

	struct ThreadPrivate;

	/** 
	 * Class implementing a thread. 
	 */
	class PULSAR_PUBLIC Thread : public ErrorHandle 
	{
		public:
			Thread(handle_t handle = -1);
			~Thread();

			void SetName(const char *name);
			bool Run();
			bool Wait(useconds_t timeout = -1) const;
			void Quit(int status = 0);

			bool IsRunning() const;
			int GetStatus() const;
			thread_id_t GetID() const;

			/** 
			 * Signal emitted when the thread exits.
			 *
			 * @param		Exit status code. 
			 */
			Signal<int> OnExit;

			static thread_id_t GetCurrentID();
			static void Sleep(useconds_t usecs);

		protected:
			EventLoop &GetEventLoop();
			virtual int Main();

		private:
			void RegisterEvents();
			void HandleEvent(int event);

			PULSAR_PRIVATE static void _Entry(void *arg);

			ThreadPrivate *m_priv;		// Internal data pointer
	};

}

#endif // __PULSAR_THREAD_H