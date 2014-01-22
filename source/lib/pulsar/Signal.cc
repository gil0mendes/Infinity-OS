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
 * @brief 	Type-safe callback system
 */

#include <Signal.h>

using namespace pulsar::internal;
using namespace pulsar;

/**
 * Destroy a slot, removing it from the list.
 */
SignalImpl::Slot::~Slot()
{
	m_impl->Remove(this);
}

/**
 * Construct a new iterator
 */
SignalImpl::Iterator::Iterator(SignalImpl *impl) : m_impl(impl)
{
	m_iter = new SlotList::const_iterator(m_impl->m_slots->begin());
}

/**
 * Destroy an iterator.
 */
SignalImpl::Iterator::~Iterator()
{
	delete m_iter;
}

/**
 * Get the next slot from an iterator.
 *
 * @return 		Pointer to next slot, or NULL if no more slots.
 */
SignalImpl::Slot *SignalImpl::Iterator::operator *()
{
	if (*m_iter == m_impl->m_slots->end())
	{
		return 0;
	}

	return *((*m_iter)++);
}

/**
 * Construct a signal implementation object.
 */
SignalImpl::SignalImpl()
{
	m_slots = new SlotList();
}

/**
 * Destroy a signal, freeing all slots in the list
 */
SignalImpl::~SignalImpl()
{
	SlotList::iterator it;

	while((it = m_slots->begin()) != m_slots->end())
	{
		delete *it;
	}

	delete m_slots;
}

/**
 * Insert a slot into the signal's list.
 *
 * @param slot 	Slot to add
 */
void SignalImpl::Insert(Slot *slot)
{
	m_slots->remove(slot);
}
