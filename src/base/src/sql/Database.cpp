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

#include "sql/Database.h"

#include <sqlite3.h>

#include "Exception.h"

namespace {
class SQLException : public kyla::RuntimeException
{
public:
	SQLException (const std::string& what, const char* file, const int line)
		: RuntimeException (what.c_str (), file, line)
	{
	}

	SQLException (sqlite3* db, const int r, const char* file, const int line)
		: SQLException (std::string (sqlite3_errstr (r)) + ":" + std::string (sqlite3_errmsg (db)), file, line)
	{
	}
};
}

#define SAFE_SQLITE_INTERNAL(expr, file, line) do { const int r_ = (expr); if (r_ != SQLITE_OK) { throw  SQLException (db_, r_, file, line); } } while (0)

#define K_S(expr) SAFE_SQLITE_INTERNAL(expr, __FILE__, __LINE__)

namespace kyla {
namespace Sql {
struct Database::Impl
{
public:
	Impl () = default;

	Impl (const Impl&) = delete;
	Impl& operator=(const Impl&) = delete;

	Impl (Impl&& other)
		: db_ (other.db_)
	{
		other.db_ = nullptr;
	}

	Impl& operator= (Impl&& other)
	{
		db_ = other.db_;
		other.db_ = nullptr;
	}

	void Open (const char *name, const OpenMode mode)
	{
		int sqliteOpenMode = 0;
		switch (mode) {
		case OpenMode::Read:
			sqliteOpenMode = SQLITE_OPEN_READONLY;
			break;

		case OpenMode::ReadWrite:
			sqliteOpenMode = SQLITE_OPEN_READWRITE;
			break;
		}

		K_S (sqlite3_open_v2(name, &db_, sqliteOpenMode, nullptr));
	}

	void Create (const char* name)
	{
		K_S(sqlite3_open_v2 (name, &db_,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr));
	}

	void Create ()
	{
		K_S(sqlite3_open (":memory:", &db_));
	}

	void Close ()
	{
		if (db_) {
			K_S(sqlite3_close (db_));
			db_ = nullptr;
		}
	}

	~Impl ()
	{
		if (db_) {
			sqlite3_close (db_);
		}
	}

	void TransactionBegin ()
	{
		K_S(sqlite3_exec (db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr));
	}

	void TransactionCommit ()
	{
		K_S(sqlite3_exec (db_, "COMMIT;", nullptr, nullptr, nullptr));
	}

	void TransactionRollback ()
	{
		K_S(sqlite3_exec (db_, "ROLLBACK;", nullptr, nullptr, nullptr));
	}

	void StatementPrepare (const char* sql, void** result)
	{
		sqlite3_stmt* stmt;
		K_S(sqlite3_prepare_v2 (db_, sql, -1, &stmt, nullptr));
		*result = static_cast<void*> (stmt);
	}

	void StatementBind (void* statement, const int index,
		const std::int64_t value)
	{
		K_S (sqlite3_bind_int64(static_cast<sqlite3_stmt*>(statement), index,
			value));
	}

	void StatementBind (void* statement, const int index,
		const char* value, const ValueBinding binding)
	{
		K_S (sqlite3_bind_text(static_cast<sqlite3_stmt*>(statement), index,
			value, -1, SQLiteValueBinding (binding)));
	}

	void StatementBind (void* statement, const int index,
		const Null&)
	{
		K_S (sqlite3_bind_null (static_cast<sqlite3_stmt*>(statement), index));
	}

	void StatementBind (void* statement, const int index,
		const std::size_t size, const void* data,
		const ValueBinding binding)
	{
		///@TODO(minor) Check for overflow
		K_S (sqlite3_bind_blob (static_cast<sqlite3_stmt*>(statement), index,
			data, static_cast<int> (size), SQLiteValueBinding (binding)));
	}

	bool StatementStep (void* statement)
	{
		auto r = sqlite3_step (static_cast<sqlite3_stmt*> (statement));

		if (r == SQLITE_ROW) {
			return true;
		} else if (r == SQLITE_DONE) {
			return false;
		}

		throw SQLException (db_, r, KYLA_FILE_LINE);
	}

	void StatementReset (void* statement)
	{
		K_S (sqlite3_reset (static_cast<sqlite3_stmt*> (statement)));
	}

	void StatementFinalize (void* statement)
	{
		///@TODO This should not throw because it's called from destructur
		K_S (sqlite3_finalize (static_cast<sqlite3_stmt*> (statement)));
	}

	std::int64_t StatementGetInt64 (void* statement, const int column)
	{
		return sqlite3_column_int64(static_cast<sqlite3_stmt*> (statement), column);
	}

	const char* StatementGetText (void* statement, const int column)
	{
		return reinterpret_cast<const  char*> (
			sqlite3_column_text(static_cast<sqlite3_stmt*> (statement), column));
	}

	const void* StatementGetBlob (void* statement, const int column)
	{
		return sqlite3_column_blob (static_cast<sqlite3_stmt*> (statement), column);
	}

	void StatementGetBlob (void* statement, const int column,
		const MutableArrayRef<>& result)
	{
		if (sqlite3_column_bytes (static_cast<sqlite3_stmt*> (statement), column) != result.GetSize ()) {
			throw std::runtime_error ("Output buffer size does not match blob size");
		}

		::memcpy (result.GetData (),
			sqlite3_column_blob (static_cast<sqlite3_stmt*> (statement), column),
			result.GetSize ());
	}

	const Type StatementGetColumnType (void* statement, const int column)
	{
		const auto t = sqlite3_column_type (static_cast<sqlite3_stmt*> (statement), column);
		switch (t) {
		case SQLITE_NULL:
			return Type::Null;
		case SQLITE_INTEGER:
			return Type::Int64;
		case SQLITE_TEXT:
			return Type::Text;
		case SQLITE_BLOB:
			return Type::Blob;
		}

		throw RuntimeException ("Invalid column type",
			KYLA_FILE_LINE);
	}

	bool Execute (const char* statement)
	{
		K_S (sqlite3_exec (db_, statement, nullptr, nullptr, nullptr));
		return true;
	}

	void SaveCopyTo (const char* filename)
	{
		sqlite3* targetDb;
		sqlite3_open (filename, &targetDb);
		auto backup = sqlite3_backup_init(targetDb, "main",
			db_, "main");
		sqlite3_backup_step (backup, -1);
		sqlite3_backup_finish (backup);
		sqlite3_close (targetDb);
	}

	std::int64_t GetLastRowId ()
	{
		return sqlite3_last_insert_rowid (db_);
	}

	void AttachTemporaryCopy (Impl* other, const char* name)
	{
		std::string sql = "ATTACH DATABASE ':memory:' AS ";
		sql += name;
		K_S (sqlite3_exec (db_, sql.c_str (), nullptr, nullptr, nullptr));

		auto backup = sqlite3_backup_init (db_, name, other->db_, "main");

		if (backup == nullptr) {
			throw SQLException (db_, sqlite3_errcode (db_), KYLA_FILE_LINE);
		}

		sqlite3_backup_step (backup, -1);
		sqlite3_backup_finish (backup);
	}

	void Detach (const char* name)
	{
		std::string sql = "DETACH DATABASE ";
		sql += name;
		sqlite3_exec (db_, sql.c_str (), nullptr, nullptr, nullptr);
	}

private:
	// Returns a function pointer to a void (void*) function
	static void(*SQLiteValueBinding(const ValueBinding binding))(void*)
	{
		switch (binding) {
		case ValueBinding::Copy:
			return SQLITE_TRANSIENT;
		case ValueBinding::Reference:
			return SQLITE_STATIC;
		}

		return nullptr;
	}

	sqlite3* db_ = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
Database::Database ()
	: impl_ (new Impl)
{
}

////////////////////////////////////////////////////////////////////////////////
Database::Database (Database&& other)
	: impl_ (std::move (other.impl_))
{
}

////////////////////////////////////////////////////////////////////////////////
Database& Database::operator =(Database&& other)
{
	impl_ = std::move(other.impl_);
	return *this;
}

////////////////////////////////////////////////////////////////////////////////
void Database::SaveCopyTo(const char* filename) const
{
	impl_->SaveCopyTo (filename);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Open (const char* name)
{
	return Open (name, OpenMode::Read);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Open (const char* name, const OpenMode openMode)
{
	Database db;
	db.impl_->Open (name, openMode);
	return std::move (db);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Open (const Path& path)
{
	return Open (path, OpenMode::Read);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Open (const Path& path, const OpenMode openMode)
{
	Database db;
	db.impl_->Open (path.string ().c_str (), openMode);
	return std::move (db);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Create (const char* name)
{
	Database db;
	db.impl_->Create (name);
	return std::move (db);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Create ()
{
	Database db;
	db.impl_->Create ();
	return std::move (db);
}

////////////////////////////////////////////////////////////////////////////////
void Database::Close ()
{
	impl_->Close ();
}

////////////////////////////////////////////////////////////////////////////////
bool Database::Execute (const char* statement)
{
	return impl_->Execute (statement);
}

////////////////////////////////////////////////////////////////////////////////
Transaction::Transaction (Database::Impl* impl)
: impl_ (impl)
{
	impl_->TransactionBegin ();
}

////////////////////////////////////////////////////////////////////////////////
Transaction::~Transaction ()
{
	if (impl_) {
		impl_->TransactionRollback ();
	}
}

////////////////////////////////////////////////////////////////////////////////
void Transaction::Commit ()
{
	impl_->TransactionCommit ();
	impl_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
void Transaction::Rollback ()
{
	impl_->TransactionRollback ();
	impl_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
Statement::Statement (Database::Impl* impl, const char* statement)
: impl_ (impl)
{
	impl_->StatementPrepare (statement, &p_);
}

////////////////////////////////////////////////////////////////////////////////
Statement::~Statement ()
{
	if (p_) {
		impl_->StatementFinalize (p_);
	}
}

////////////////////////////////////////////////////////////////////////////////
Statement::Statement (Statement&& other)
	: impl_ (other.impl_)
	, p_ (other.p_)
{
	other.p_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
Statement& Statement::operator=(Statement&& other)
{
	impl_ = other.impl_;
	p_ = other.p_;

	other.p_ = nullptr;

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
bool Statement::Step ()
{
	return impl_->StatementStep (p_);
}

////////////////////////////////////////////////////////////////////////////////
void Statement::Reset ()
{
	return impl_->StatementReset (p_);
}

////////////////////////////////////////////////////////////////////////////////
void Statement::Bind (const int index, const std::int64_t value)
{
		impl_->StatementBind (static_cast<sqlite3_stmt*> (p_), index, value);
}

////////////////////////////////////////////////////////////////////////////////
void Statement::Bind (const int index, const char* value,
	const ValueBinding binding)
{
		impl_->StatementBind (static_cast<sqlite3_stmt*> (p_), index, value, binding);
}

////////////////////////////////////////////////////////////////////////////////
void Statement::Bind (const int index, const std::string& value)
{
	Bind (index, value.c_str ());
}

////////////////////////////////////////////////////////////////////////////////
void Statement::Bind (const int index, const Null&)
{
	impl_->StatementBind (static_cast<sqlite3_stmt*> (p_), index, Null());
}

////////////////////////////////////////////////////////////////////////////////
void Statement::Bind (const int index,
	const ArrayRef<>& data, const ValueBinding binding)
{
	impl_->StatementBind (static_cast<sqlite3_stmt*> (p_), index,
		data.GetSize (), data.GetData (), binding);
}

////////////////////////////////////////////////////////////////////////////////
std::int64_t Statement::GetInt64 (const int index) const
{
	return impl_->StatementGetInt64 (p_, index);
}

////////////////////////////////////////////////////////////////////////////////
const char* Statement::GetText (const int index) const
{
	return impl_->StatementGetText (p_, index);
}

////////////////////////////////////////////////////////////////////////////////
const void* Statement::GetBlob (const int index) const
{
	return impl_->StatementGetBlob (p_, index);
}

////////////////////////////////////////////////////////////////////////////////
void Statement::GetBlob (const int index, const MutableArrayRef<>& result) const
{
	return impl_->StatementGetBlob (p_, index, result);
}

////////////////////////////////////////////////////////////////////////////////
Type Statement::GetColumnType (const int index) const
{
	return impl_->StatementGetColumnType (p_, index);
}

////////////////////////////////////////////////////////////////////////////////
Transaction Database::BeginTransaction(TransactionType type)
{
	return Transaction (impl_.get ());
}

////////////////////////////////////////////////////////////////////////////////
Statement Database::Prepare (const char* statement)
{
	return Statement (impl_.get (), statement);
}

////////////////////////////////////////////////////////////////////////////////
Statement Database::Prepare (const std::string& statement)
{
	return Prepare (statement.c_str ());
}

////////////////////////////////////////////////////////////////////////////////
std::int64_t Database::GetLastRowId()
{
	return impl_->GetLastRowId ();
}

////////////////////////////////////////////////////////////////////////////////
void Database::AttachTemporaryCopy (const char* name, Database & source)
{
	impl_->AttachTemporaryCopy (source.impl_.get (), name);
}

////////////////////////////////////////////////////////////////////////////////
void Database::Detach (const char * name)
{
	impl_->Detach (name);
}

////////////////////////////////////////////////////////////////////////////////
Transaction::Transaction (Transaction&& other)
	: impl_ (other.impl_)
{
	other.impl_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
Transaction& Transaction::operator=(Transaction&& other)
{
	impl_ = other.impl_;
	other.impl_ = nullptr;

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
Database::~Database ()
{
}
}
}
