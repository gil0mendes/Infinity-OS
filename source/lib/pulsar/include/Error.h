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
 * @brief 	Error class
 */

#ifndef __PULSAR_ERROR_H
#define __PULSAR_ERROR_H

#include <kernel/status.h>

#include <CoreDefs.h>
#include <exception>

namespace pulsar
{

	/**
	 * Base class for erros
	 */
	class PULSAR_PUBLIC BaseError : public std::exception
	{
		public:
			virtual const char *GetDescription() const throw() = 0;
			virtual const char *GetRecoverySuggestion() const throw();

		private:
			const char *what() const throw();
	};

	/**
	 * Class providing information on an error.
	 *
	 * THis class is used to report errors from API functions. It is a wrapper
	 * around status_t which allows information to be obtained about an error,
	 * such as a human-redable error description and sugestions for recovering
	 * from an error.
	 *
	 * The suggested method for using this class in classes not designed to be
	 * used from multiple threads simultaneously is to return a bool stating 
	 * whether or not the function succeeded, and to have a GetError function 
	 * that returns a reference to an error object giving details of the error.
	 * The suggested method for using this class in classes designed to be used
	 * from multiple threads simultaneously is to return a bool stating whether
	 * or not the function succeeded, and take an optional pointer to an Error 
	 * object in which error information will be stored.
	 */
	class PULSAR_PUBLIC Error : public BaseError
	{
		public:
			Error() throw() : m_code(STATUS_SUCCESS) {}
			Error(status_t code) throw() : m_code(code) {}

			bool operator ==(Error &other) const throw() { return other.m_code == m_code; }
			bool operator ==(status_t code) const throw() { return code == m_code; }

			/**
			 * Get the status code.
			 *
			 * @return COde describing the error
			 */
			status_t GetCode() const throw() { return m_code; }

			virtual const char *GetDescription() const throw();
			virtual const char *GetRecoverySuggestion() const throw();

		private:
			status_t m_code;	// Status code
	};
}

#endif // __PULSAR_ERROR_H
