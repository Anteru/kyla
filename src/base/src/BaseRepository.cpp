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
std::vector<Uuid> BaseRepository::GetFeaturesImpl ()
{
	static const char* querySql =
		"SELECT file_sets.Uuid FROM file_sets;";

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
bool BaseRepository::IsEncryptedImpl ()
{
	static const char* querySql =
		"SELECT EXISTS(SELECT 1 FROM fs_chunk_encryption);";

	auto query = GetDatabase ().Prepare (querySql);
	query.Step ();

	return query.GetInt64 (0) != 0;
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::SetDecryptionKeyImpl (const std::string& key)
{
	key_ = key;
}

///////////////////////////////////////////////////////////////////////////////
std::string BaseRepository::GetDecryptionKeyImpl () const
{
	return key_;
}

///////////////////////////////////////////////////////////////////////////////
int64_t BaseRepository::GetFeatureSizeImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT SUM(fs_contents.size) "
		"FROM file_sets "
		"INNER JOIN fs_files ON features.Id = fs_files.FeatureId "
		"INNER JOIN fs_contents ON fs_contents.Id = files.ContentId "
		"WHERE features.Uuid = ?";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetInt64 (0);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::RepairImpl (Repository& /*source*/,
	ExecutionContext& /*context*/,
	RepairCallback /*repairCallback*/,
	bool /*restore*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}

///////////////////////////////////////////////////////////////////////////////
void BaseRepository::ConfigureImpl (Repository& /*other*/,
	const ArrayRef<Uuid>& /*features*/,
	ExecutionContext& /*context*/)
{
	throw RuntimeException ("NOT IMPLEMENTED", KYLA_FILE_LINE);
}
}