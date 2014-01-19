/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Type management.
 */

#ifndef __TYPE_H
#define __TYPE_H

#include <list>
#include <map>
#include <string>
#include <utility>

/** Base class for a type. */
class Type {
public:
	Type(const char *name) : m_name(name) {}
	virtual ~Type() {};
	virtual void Dump() const;

	/** Get the name of the type.
	 * @return		Reference to type name. */
	const std::string &GetName() const { return m_name; }
protected:
	std::string m_name;		/**< Name of the type. */
};

/** Class for a boolean type. */
class BooleanType : public Type {
public:
	BooleanType(const char *name) : Type(name) {}
};

/** Class for a string type. */
class StringType : public Type {
public:
	StringType(const char *name) : Type(name) {}
};

/** Class for an arbitrary data string type. */
class BytesType : public Type {
public:
	BytesType(const char *name) : Type(name) {}
};

/** Class for an integer type. */
class IntegerType : public Type {
public:
	/** Construct the type.
	 * @param name		Name of the type.
	 * @param width		Width of the type in bits.
	 * @param is_signed	Whether the type is signed. */
	IntegerType(const char *name, size_t width, bool is_signed) :
		Type(name), m_width(width), m_is_signed(is_signed)
	{}

	virtual void Dump() const;

	/** Get the width of the type.
	 * @return		Width of the type. */
	size_t GetWidth() const { return m_width; }

	/** Check if the type is signed.
	 * @return		Whether the type is signed. */
	bool IsSigned() const { return m_is_signed; }
private:
	size_t m_width;			/**< Width of the type. */
	bool m_is_signed;		/**< Whether the type is signed. */
};

/** Class for a type alias. */
class AliasType : public Type {
public:
	AliasType(const char *name, Type *dest);
	virtual void Dump() const;

	/** Get the real type this alias refers to.
	 * @return		Pointer to type (never another alias. */
	Type *Resolve() const { return m_dest; }
private:
	Type *m_dest;			/**< Type the alias refers to. */
};

/** Class for a structure. */
class StructType : public Type {
public:
	/** Type of the entry list. */
	typedef std::list<std::pair<Type *, std::string> > EntryList;

	StructType(const char *name) : Type(name) {}
	virtual void Dump() const;
	bool AddEntry(Type *type, const char *name);

	/** Get the entry list.
	 * @return		Reference to entry list. */
	const EntryList &GetEntries() const { return m_entries; }
private:
	EntryList m_entries;		/**< List of entries in the type. */
};

#endif /* __TYPE_H */
