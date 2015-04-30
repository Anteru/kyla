#include "sql/Database.h"

#include <spdlog.h>
#include <sqlite3.h>

#define SAFE_SQLITE_INTERNAL(expr, file, line) do { const int r_ = (expr); if (r_ != SQLITE_OK) { spdlog::get ("log")->error () << file << ":" << line << " " << sqlite3_errstr(r_); exit (1); } } while (0)
#define SAFE_SQLITE_INSERT_INTERNAL(expr, file, line) do { const int r_ = (expr); if (r_ != SQLITE_DONE) { spdlog::get ("log")->error () << file << ":" << line << " " << sqlite3_errstr(r_); exit (1); } } while (0)

#define K_S(expr) SAFE_SQLITE_INTERNAL(expr, __FILE__, __LINE__)
#define K_S_INSERT(expr) SAFE_SQLITE_INSERT_INTERNAL(expr, __FILE__, __LINE__)

namespace kyla {
namespace Sql {
struct Database::Impl
{
public:
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

private:
	sqlite3* db_ = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
Database::Database ()
	: impl_ (new Impl ())
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
Database Database::Open (const char* name)
{
	return Open (name, OpenMode::Read);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Open (const char* name, const OpenMode openMode)
{
	Database db;
	db.Open (name, openMode);
	return std::move (db);
}

////////////////////////////////////////////////////////////////////////////////
Database Database::Create (const char* name)
{
	Database db;
	db.Create (name);
	return std::move (db);
}

////////////////////////////////////////////////////////////////////////////////
void Database::Close ()
{
	impl_->Close ();
}

////////////////////////////////////////////////////////////////////////////////
Transaction::Transaction (Database::Impl* impl)
: impl_ (impl)
{
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
bool Statement::Step ()
{
	return sqlite3_step (static_cast<sqlite3_stmt*> (p_));
}
}
}
