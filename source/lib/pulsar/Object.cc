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

#include <Object.h>
#include <EventLoop.h>

#include <list>

#include "Internal.h"

using namespace pulsar;

/**
 * Internal data for Object
 */
struct infi::ObjectPrivate
{
	typedef std::list<internal::SignalImp::Slot *> SlotList;

	ObjectPrivate() : destroyed(false) {}

	bool destroyed; 	// Whether the object is being destroyed
	SlotList slots; 	// Slots associated with this object
};

/**
 * Contructor for Object
 *
 * @note Protected - Object cannot be instantiated directly
 */
Object::Object()
{
	m_priv = new ObjectPrivate;
}

/** 
 * Constructor for Object.
 * 
 * @note		Protected - Object cannot be instantiated directly. 
 */
Object::Object() {
	m_priv = new ObjectPrivate;
}

/** 
 * Schedule the object for deletion when control returns to the event loop. 
 */
void Object::DeleteLater() 
{
	EventLoop *loop = EventLoop::Instance();
	
	if(loop) 
	{
		loop->DeleteObject(this);
	} 
	else 
	{
		libinfi_warn("Object::DeleteLater: Called without an event loop, will not be deleted.");
	}
}

/** 
 * Add a slot to the object.
 * 
 * @param slot 	Slot to add. This slot will be removed from its signal
 *				when the object is destroyed. 
 */
void Object::AddSlot(internal::SignalImpl::Slot *slot) 
{
	m_priv->slots.push_back(slot);
}

/** 
 * Remove a slot from the object.
 * 
 * @param slot	Slot to remove. 
 */
void Object::RemoveSlot(internal::SignalImpl::Slot *slot) 
{
	if(!m_priv->destroyed) 
	{
		m_priv->slots.remove(slot);
	}
}

/** 
 * Destructor for Object. 
 */
Object::~Object() 
{
	// Call our OnDestroy signal
	try 
	{
		OnDestroy(this);
	} 
	catch(...) 
	{
		libinfi_warn("Object::~Object: Unexpected exception in OnDestroy handler.");
		throw;
	}

	// Set the destroyed flag. This is to speed up slot removal: deleting
	// a slot will cause RemoveSlot() to be called, which checks the
	// flag to see if it needs to bother removing from the list
	m_priv->destroyed = true;

	// Remove all slots pointing to this object from the Signal that they
	// are for
	for(auto it = m_priv->slots.begin(); it != m_priv->slots.end(); ++it) 
	{
		delete *it;
	}

	delete m_priv;
}
