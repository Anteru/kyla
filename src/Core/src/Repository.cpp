#include "Repository.h"

#include "sql/Database.h"
#include "FileIO.h"
#include "Hash.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void IRepository::Validate (const ValidationCallback& validationCallback)
{
	ValidateImpl (validationCallback);
}

struct DeployedRepository::Impl
{
public:
	Impl (const char* path)
		: db_ (Sql::Database::Open (Path (path) / "k.db"))
		, path_ (path)
	{
	}

	void Validate (const IRepository::ValidationCallback& validationCallback)
	{
		// Get a list of (file, hash, size)
		// We sort by size first so we get small objects out of the way first
		// (slower progress, but more things getting processed) and speed up 
		// towards the end (larger files, higher throughput)
		static const char* querySql = 
			"SELECT files.path, content_objects.Hash, content_objects.Size "
			"FROM files "
			"LEFT JOIN content_objects ON content_objects.Id = files.ContentObjectId "
			"ORDER BY size";

		///@TODO(major) On Windows, sort this by disk cluster to get best
		/// disk access pattern
		/// See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364572%28v=vs.85%29.aspx

		auto query = db_.Prepare (querySql);

		while (query.Step ()) {
			const Path path = query.GetText (0);
			SHA256Digest hash;
			query.GetBlob (1, hash);
			const auto size = query.GetInt64 (2);

			const auto filePath = path_ / path;
			if (!boost::filesystem::exists (filePath)) {
				validationCallback (filePath.string ().c_str (),
					kylaValidationResult_Missing);

				continue;
			}

			const auto statResult = Stat (filePath);

			///@TODO Try/catch here and report corrupted if something goes wrong?
			/// This would indicate the file got deleted or is read-protected
			/// while the validation is running

			if (statResult.size != size) {
				validationCallback (filePath.string ().c_str (),
					kylaValidationResult_Corrupted);

				continue;
			}

			// For size 0 files, don't bother checking the hash
			///@TODO Assert hash is the null hash
			if (size != 0 && ComputeSHA256 (filePath) != hash) {
				validationCallback (filePath.string ().c_str (),
					kylaValidationResult_Corrupted);

				continue;
			}

			validationCallback (filePath.string ().c_str (),
				kylaValidationResult_Ok);
		}
	}

private:
	Sql::Database db_;
	Path path_;
};

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::~DeployedRepository ()
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::DeployedRepository (const char* path)
	: impl_ (new Impl { path })
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::DeployedRepository (DeployedRepository&& other)
	: impl_ (std::move (other.impl_))
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository& DeployedRepository::operator= (DeployedRepository&& other)
{
	impl_ = std::move (other.impl_);
	return *this;
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::ValidateImpl (const ValidationCallback& validationCallback)
{
	impl_->Validate (validationCallback);
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IRepository> OpenRepository (const char* path)
{
	// For know, we only support deployed repositories
	return std::unique_ptr<IRepository> (new DeployedRepository{ path });
}
}