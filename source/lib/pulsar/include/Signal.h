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
 * @brief 	Type-sabe callback system
 */

#ifndef __PULSAR_SIGNAL_H
#define __PULSAR_SIGNAL_H

#include <CoreDefs.h>

#include <type_traits>
#include <list>

namespace pulsar
{
	namespace internal
	{
		/**
		 * Internal signal implementation
		 */
		class PULSAR_PUBLIC SignalImpl
		{
			public:
				/**
				 * Base class for slot
				 */
				class Slot
				{
					public:
						Slot(SignalImpl *impl) : m_impl(impl) {}
						virtual ~Slot();

					private:
						SignalImpl *m_impl;
				};

				// TODO: Replace this.
				typedef std::list<Slot *> SlotList;

				/**
				 * Iterator class
				 */
				class Iterator
				{
					public:
						Iterator(SignalImpl *impl);
						~Iterator();

						Slot *operator *();

					private:
						SignalImpl *m_impl;
						SlotList::const_iterator *m_iter;
				};

				SignalImpl();
				~SignalImpl();

				void Insert(Slot *slot);
				void Remove(Slot *slot);

			private:
				SlotList *m_slots;
				friend class Iterator;
		};
	}

	class Object;

	/**
	 * CLass implementation a type-safe callback system
	 */
	template <typename... A>
	class Signal : public internal::SignalImpl
	{
		/**
		 * Base class for a slot
		 */
		class Slot : public SignalImpl::Slot
		{
			public:
				Slot(SignalImpl *impl) : SignalImpl::Slot(impl) {}
				virtual void operator ()(A...) = 0;
		};

		/**
		 * Slot for a non-member function
		 */
		class RegularSlot : public Slot
		{
			public:
				RegularSlot(SignalImpl *impl, void (*func)(A...)) : Slot(impl), m_func(func) {}
				void operator ()(A... args) 
				{
					m_func(args...);
				}

			private:
				void (*m_func)(A...);
		};

		/**
		 * Slot for a member function
		 */
		template <typename T, bool object = std::is_base_of<Object, T>::value>
		class MemberSlot : public Slot
		{
			public:
				MemberSlot(SignalImpl *impl, T *obj, void (T::*func)(A...)) : 
					Slot(impl), m_obj(obj), m_func(func) 
				{}

				void operator ()(A... args)
				{
					(m_obj->*m_func)(args...);
				}

			private:
				T *m_obj;
				void (T::*m_func)(A...);
		};

		/**
		 * Slot for a member function in an Object-derived class
		 */
		template <typename T>
		class MemberSlot<T, true> : public Slot
		{
			public:
				MemberSlot(SignalImpl *impl, T *obj, void (T::*func)(A...)) : 
					Slot(impl), m_obj(obj), m_func(func)
				{
					/**
				 	* Tell the Object about this slot, so that when the
				 	* object is destroyed the slot will automatically be
				 	* removed from the signal
				 	*/
					m_obj->AddSlot(this);
				}

				~MemberSlot()
				{
					// Remove thr slot from the object
					m_obj->RemoveSlot(this);
				}

				void operator ()(A... args)
				{
					(m_obj->*m_func)(args...);
				}

			private:
				T *m_obj;
				void (T::*m_func)(A...);
		};

		public:
		/**
		 * Connect a function to this signal.
		 *
		 * @param func 	Function to call
		 */
		void connect(void (*func)(A...))
		{
			Insert(static_cast<SignalImpl::Slot *>(new RegularSlot(this, func)));
		}

		/**
		 * Connect a member function to this signal.
		 *
		 * @param obj 	Object to call function on.
		 * @param func 	Function to call
		 */
	   template <typename T>
	   void connect(T *obj, void (T::*func)(A...))
	   {
		   Insert(static_cast<SignalImpl::Slot *>(new MemberSlot<T>(this, obj, func)));
	   }   

	   /**
		* Connect this signal to another signal.
		*
		* @param signal 	Other signal to emit when emitted
		*/
	   void connect(Signal<A...> &signal)
	   {
		   connect(&signal, &Signal<A...>::operator ());
	   }

	   /**
		* Incoke all slots connected to the signal
		*
		* @param args 	Arguments to pass to the slot
		*/
	   void operator ()(A... args)
	   {
		   SignalImpl::Iterator it(this);
		   SignalImpl::Slot *slot;

		   while((slot = *it))
		   {
			   (*static_cast<Slot *>(slot)(args...));
		   }
	   }

	};
}

#endif // __PULSAR_SIGNAL_H
