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
 * @brief	Handle class.
 */

#ifndef __PULSAR_HANDLE_H
#define __PULSAR_HANDLE_H

#include <Support/Noncopyable.h>
#include <Error.h>
#include <Object.h>

namespace pulsar
{

	class EventLoop;

	// Base class for all objects accessed through a handle
	class PULSAR_PUBLIC Handle : public Object, Noncopyable 
	{
		friend class EventLoop;

		public:
			virtual ~Handle();

			void Close();
			void InhibitEvents(bool inhibit);

			/**
			 * Get the kernel handle for this object
			 *
			 * @return	Kernel handle, or -1 if not currently open. Do
			 *			NOT close the returned handle. 
			 */
			handle_t GetHandle() const { return m_handle; }

			// Signal emitted when the handle is closed
			Signal<> OnClose;

		protected:
			Handle();

			void SetHandle(handle_t handle);
			void RegisterEvent(int event);
			void UnregisterEvent(int event);
			status_t _Wait(unsigned event, useconds_t timeout) const;

			virtual void RegisterEvents();
			virtual void HandleEvent(int event);

			handle_t m_handle; // Handle ID

		private:
			EventLoop *m_event_loop;	// Event loop handling this handle
	};

	/** 
	 * Base handle class with an Error object
	 *
	 * @note	See documentation for Error for when to use this
	 */
	class PULSAR_PUBLIC ErrorHandle : public Handle 
	{
		public:
			/** 
			 * Get information about the last error that occurred
			 *
			 * @return	Reference to error object for last error. 
			 */
			const Error &GetError() const { return m_error; }

		protected:
			/** 
			 * Set the error information
			 *
			 * @param code	Status code to set
			 */
			void SetError(status_t code) { m_error = code; }

			/** 
			 * Set the error information
			 *
			 * @param error		Error object to set
			 */
			void SetError(const Error &error) { m_error = error.GetCode(); }
			
		private:
			Error m_error;	// Error information
	};

}

#endif // __PULSAR_HANDLE_H