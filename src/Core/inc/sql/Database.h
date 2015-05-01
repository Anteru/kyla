#ifndef KYLA_CORE_INTERNAL_DATABASE_H
#define KYLA_CORE_INTERNAL_DATABASE_H

#include <memory>
#include <cstdint>
#include <string>

namespace kyla {
namespace Sql {
enum class OpenMode
{
	Read,
	ReadWrite
};

class Statement;
class Transaction;

enum class TransactionType
{
	Deferred,
	Immediate
};

class Database
{
	Database (const Database& other) = delete;
	Database& operator= (const Database&) = delete;

public:
	Database ();

	Database (Database&& other);
	Database& operator=(Database&& other);

	static Database Open (const char* name);
	static Database Open (const char* name, const OpenMode openMode);

	static Database Create (const char* name);
	static Database Create ();

	void Close ();

	~Database ();

	Transaction BeginTransaction (TransactionType type = TransactionType::Immediate);

	Statement Prepare (const char* statement);

	bool Execute (const char* statement);

	void SaveCopyTo (const char* filename) const;

	int GetLastRowId ();

public:
	struct Impl;

private:
	std::unique_ptr<Impl> impl_;
};

class Transaction
{
public:
	Transaction (const Transaction&) = delete;
	Transaction& operator=(const Transaction&) = delete;

	Transaction (Transaction&& statement);
	Transaction& operator=(Transaction&& statement);

	Transaction (Database::Impl* impl);
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

class Statement
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
	void Bind (const int index, const std::size_t size,
		const void* data,
		const ValueBinding binding = ValueBinding::Copy);

	template <typename ... Args>
	void BindArguments (Args&& ... args)
	{
		BindArgumentsInternal<1> (args ...);
	}

	///@TODO Bind ArrayRef
	std::int64_t GetInt64 (const int column) const;
	const char* GetText (const int column) const;
	const void* GetBlob (const int column) const;

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
}
}

#endif
