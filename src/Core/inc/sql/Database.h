#ifndef KYLA_CORE_INTERNAL_DATABASE_H
#define KYLA_CORE_INTERNAL_DATABASE_H

#include <memory>
#include <cstdint>

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

	void Close ();

	~Database ();

	Transaction BeginTransaction (TransactionType type = TransactionType::Immediate);

	Statement Prepare (const char* statement);
public:
	struct Impl;

private:
	std::unique_ptr<Impl> impl_;
};

class Transaction
{
public:
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
	Statement (Database::Impl* impl, const char* statement);
	~Statement ();

	void Bind (const int index, const std::int64_t value);
	void Bind (const int index, const char* value,
		const ValueBinding binding = ValueBinding::Copy);
	void Bind (const int index, const Null&);
	void Bind (const int index, const std::size_t size,
		const void* data,
		const ValueBinding binding = ValueBinding::Copy);

	///@TODO Bind ArrayRef

	bool Step ();
	void Reset ();

private:
	Database::Impl* impl_ = nullptr;
	void*			p_ = nullptr;
};
}
}

#endif
