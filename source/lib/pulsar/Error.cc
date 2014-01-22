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
 * @brief 	Error handling classes
 */

#include <Error.h>

using namespace pulsar;

/**
 * Get a description of the error.
 *
 * @return String error description
 */
const char *BaseError::what() const throw()
{
	return GetDescription();
}

/**
 * Get a recovery suggestion for the error.
 *
 * @return 	Localized string suggestion a recovery acction for the error.
 * 			If no suggestion is available, an empty string will be returned.
 */
const char *BaseError::GetRecoverySuggestion() const throw()
{
	return "";
}

// ---------- Error

/**
 * Get the string description of the error.
 * 
 * @return      Localised string describing the error that occurred.
 */
const char *Error::GetDescription() const throw()
{
	if (m_code < 0 || static_cast<size_t>(m_code) >= __kernel_status_size)
	{
		return "Unknown error";
	}
	else if (!__kernel_status_strings[m_code])
	{
		return "Unknown error";
	}

	return __kernel_status_strings[m_code];
}

/**
 * Get a recovery suggestion for the error.
 *
 * @return 	Localised string suggesting a recovery action for the error.
 * 			If no suggestion is available, an empty string will be returned.
 */
const char *Error::GetRecoverySuggestion() const throw()
{
	// TODO
	return "";
}

