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
 * @brief	Event loop class.
 */

#ifndef __PULSAR_EVENTLOOP_H
#define __PULSAR_EVENTLOOP_H

#include <Handle.h>

namespace pulsar
{

	class Thread;
	struct EventLoopPrivate;

	/** 
	 * Class implementing a loop for handling object events. 
	 */
	class PULSAR_PUBLIC EventLoop : public Object, Noncopyable 
	{
		friend class Object;
		friend class Thread;

		public:
			EventLoop();
			~EventLoop();

			void AttachHandle(Handle *handle);
			void DetachHandle(Handle *handle);

			void AddEvent(Handle *handle, unsigned event);
			void RemoveEvent(Handle *handle, unsigned event);
			void RemoveEvents(Handle *handle);

			virtual void PreHandle();
			virtual void PostHandle();

			int Run();
			void Quit(int status = 0);

			static EventLoop *Instance();

		private:
			PULSAR_PRIVATE EventLoop(bool priv);
			PULSAR_PRIVATE void Merge(EventLoop *old);
			PULSAR_PRIVATE void DeleteObject(Object *obj);

			EventLoopPrivate *m_priv;	// Internal data pointer
	};

}

#endif // __PULSAR_EVENTLOOP_H
