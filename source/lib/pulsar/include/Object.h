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
 * @brief 	API object base class
 */

#ifndef __PULSAR_OBJECT_H
#define __PULSAR_OBJECT_H

#include <Signal.h>

namespace pulsar
{
	struct ObjectPrivate;

	/**
	 * Base class for an API object
	 */
	class PULSAR_PUBLIC Object
	{
		public:
			virtual ~Object();

			void DeleteLater();

			void AddSlot(internal::SignalImpl::Slot *slot);
			void RemoveSlot(internal::SignalImpl::Slot *slot);

			/**
			 * Signal emitted when the object is destroyed.
			 *
			 * @note 	Handlers should NOT throw any exceptions
			 * @param	Pointer to the object
			 */
			Signal<Object *> OnDestroy;

		protected:
			Object();

		private:
			ObjectPrivate *m_priv; 	// Internal data for the object
	};
}

#endif // __PULSAR_OBJECT_H
