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

#include <unordered_map>

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
std::vector<Uuid> BaseRepository::GetFeaturesImpl ()
{
	static const char* querySql =
		"SELECT Uuid FROM features;";

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
int64_t BaseRepository::GetFeatureSizeImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT Size FROM feature_fs_contents_size WHERE Uuid=?;";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetInt64 (0);
}

///////////////////////////////////////////////////////////////////////////////
std::string BaseRepository::GetFeatureTitleImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT Title FROM features WHERE Uuid=?;";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	if (query.GetText (0)) {
		return query.GetText (0);
	} else {
		return std::string ();
	}
}

///////////////////////////////////////////////////////////////////////////////
std::string BaseRepository::GetFeatureDescriptionImpl (const Uuid& id)
{
	static const char* querySql =
		"SELECT Description FROM features WHERE Uuid=?;";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	if (query.GetText (0)) {
		return query.GetText (0);
	} else {
		return std::string ();
	}
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

///////////////////////////////////////////////////////////////////////////////
std::vector<Uuid> BaseRepository::GetSubfeaturesImpl (const Uuid& featureId)
{
	auto& db = GetDatabase ();

	auto selectSubfeatureQuery = db.Prepare (
		"SELECT Uuid FROM features WHERE ParentId = (SELECT Id FROM features WHERE Uuid=?);");
	selectSubfeatureQuery.BindArguments (featureId);

	std::vector<Uuid> result;
	while (selectSubfeatureQuery.Step ()) {
		Uuid uuid;
		selectSubfeatureQuery.GetBlob (0, uuid);
		result.push_back (uuid);
	}

	return result;
}
}