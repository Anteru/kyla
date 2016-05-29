/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_DATABASE_H
#define KYLA_CORE_INTERNAL_DATABASE_H

#include <memory>
#include <cstdint>
#include <string>
#include "../ArrayRef.h"
#include "../FileIO.h"

namespace kyla {
namespace Sql {
enum class OpenMode
{
	Read,
	ReadWrite
};

class Statement;
class TemporaryTable;
class Transaction;

enum class TransactionType
{
	Deferred,
	Immediate
};

class Database final
{
	Database (const Database& other) = delete;
	Database& operator= (const Database&) = delete;

public:
	Database ();

	Database (Database&& other);
	Database& operator=(Database&& other);

	static Database Open (const char* name);
	static Database Open (const char* name, const OpenMode openMode);

	static Database Open (const Path& path);
	static Database Open (const Path& path, const OpenMode openMode);

	static Database Create (const char* name);
	static Database Create ();

	void Close ();

	~Database ();

	Transaction BeginTransaction (TransactionType type = TransactionType::Immediate);

	Statement Prepare (const char* statement);
	Statement Prepare (const std::string& statement);

	bool Execute (const char* statement);

	void SaveCopyTo (const char* filename) const;

	std::int64_t GetLastRowId ();

	void AttachTemporaryCopy (const char* name, Database& source);
	void Detach (const char* name);

	TemporaryTable CreateTemporaryTable (const char* name,
		const char* columnDefinition);

public:
	struct Impl;

private:
	std::unique_ptr<Impl> impl_;
};

class Transaction final
{
public:
	Transaction (const Transaction&) = delete;
	Transaction& operator=(const Transaction&) = delete;

	Transaction (Transaction&& statement);
	Transaction& operator=(Transaction&& statement);

	explicit Transaction (Database::Impl* impl);
	Transaction (Database::Impl* impl, TransactionType type);
	~Transaction ();

	void Commit ();
	void Rollback ();

private:
	Database::Impl* impl_;
};

enum class ValueBinding
{
	Reference,
	Copy
};

struct Null
{
};

enum class Type
{
	Null,
	Int64,
	Text,
	Blob
};

class Statement final
{
public:
	Statement (const Statement&) = delete;
	Statement& operator=(const Statement&) = delete;

	Statement (Statement&& statement);
	Statement& operator=(Statement&& statement);

	Statement (Database::Impl* impl, const char* statement);
	~Statement ();

	void Bind (const int index, const std::int64_t value);
	void Bind (const int index, const std::string& value);
	void Bind (const int index, const char* value,
		const ValueBinding binding = ValueBinding::Copy);
	void Bind (const int index, const Null&);
	void Bind (const int index, const ArrayRef<>& data,
		const ValueBinding binding = ValueBinding::Copy);

	template <typename ... Args>
	void BindArguments (Args&& ... args)
	{
		BindArgumentsInternal<1> (args ...);
	}

	std::int64_t GetInt64 (const int column) const;
	const char* GetText (const int column) const;
	const void* GetBlob (const int column) const;
	void GetBlob (const int column, const MutableArrayRef<>& output) const;

	Type GetColumnType (const int column) const;

	bool Step ();
	void Reset ();

private:
	template <int Index>
	void BindArgumentsInternal ()
	{
	}

	template <int Index, typename Arg, typename ... Args>
	void BindArgumentsInternal (Arg&& arg, Args&& ... args)
	{
		Bind (Index, std::forward<Arg> (arg));
		BindArgumentsInternal<Index+1> (args...);
	}

	Database::Impl* impl_ = nullptr;
	void*			p_ = nullptr;
};

class TemporaryTable final
{
public:
	TemporaryTable (Database::Impl* impl, const char* name);
	~TemporaryTable ();

	TemporaryTable (const TemporaryTable&) = delete;
	TemporaryTable& operator=(const TemporaryTable&) = delete;

	TemporaryTable (TemporaryTable&& other);
	TemporaryTable& operator=(TemporaryTable&& other);

private:
	Database::Impl* impl_ = nullptr;
	std::string name_;
};

}
}

#endif
