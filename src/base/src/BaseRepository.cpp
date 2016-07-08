#include "BaseRepository.h"

#include "sql/Database.h"
#include "Exception.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
std::vector<FilesetInfo> BaseRepository::GetFilesetInfosImpl ()
{
	static const char* querySql =
		"SELECT file_sets.Uuid, COUNT(content_objects.Id), SUM(content_objects.size) "
		"FROM file_sets "
		"INNER JOIN files ON file_sets.Id = files.FileSetId "
		"INNER JOIN content_objects ON content_objects.Id = files.ContentObjectId "
		"GROUP BY file_sets.Uuid";

	auto query = GetDatabase ().Prepare (querySql);

	std::vector<FilesetInfo> result;

	while (query.Step ()) {
		FilesetInfo info;

		query.GetBlob (0, info.id);
		info.fileCount = query.GetInt64 (1);
		info.fileSize = query.GetInt64 (2);

		result.push_back (info);
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
std::string BaseRepository::GetFilesetNameImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT Name FROM file_sets "
		"WHERE Uuid = ?";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetText (0);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::RepairImpl (Repository& /*source*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::ValidateImpl (const ValidationCallback& /*validationCallback*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::ConfigureImpl (Repository& /*other*/,
	const ArrayRef<Uuid>& /*filesets*/,
	Log& /*log*/, Progress& /*progress*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}
}