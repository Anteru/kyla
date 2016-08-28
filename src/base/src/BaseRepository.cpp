/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "BaseRepository.h"

#include "sql/Database.h"
#include "Exception.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
std::vector<Uuid> BaseRepository::GetFilesetsImpl ()
{
	static const char* querySql =
		"SELECT file_sets.Uuid FROM file_sets ";

	auto query = GetDatabase ().Prepare (querySql);

	std::vector<Uuid> result;

	while (query.Step ()) {
		Uuid id;
		query.GetBlob (0, id);

		result.push_back (id);
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
int64_t BaseRepository::GetFilesetSizeImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT SUM(content_objects.size) "
		"FROM file_sets "
		"INNER JOIN files ON file_sets.Id = files.FileSetId "
		"INNER JOIN content_objects ON content_objects.Id = files.ContentObjectId "
		"WHERE file_sets.Uuid = ?";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetInt64 (0);
}

///////////////////////////////////////////////////////////////////////////////
int64_t BaseRepository::GetFilesetFileCountImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT COUNT(content_objects.Id) "
		"FROM file_sets "
		"INNER JOIN files ON file_sets.Id = files.FileSetId "
		"INNER JOIN content_objects ON content_objects.Id = files.ContentObjectId "
		"WHERE file_sets.Uuid = ?";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetInt64 (0);
}

///////////////////////////////////////////////////////////////////////////////
std::string BaseRepository::GetFilesetNameImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT Name FROM file_sets WHERE Uuid = ?";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetText (0);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::RepairImpl (Repository& /*source*/,
	ExecutionContext& /*context*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::ValidateImpl (const ValidationCallback& /*validationCallback*/,
	ExecutionContext& /*context*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::ConfigureImpl (Repository& /*other*/,
	const ArrayRef<Uuid>& /*filesets*/,
	ExecutionContext& /*context*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}
}