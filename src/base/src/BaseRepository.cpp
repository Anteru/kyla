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
		"SELECT Size FROM feature_fs_contents_size WHERE Uuid=?;";

	auto query = GetDatabase ().Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetInt64 (0);
}

///////////////////////////////////////////////////////////////////////////////
FeatureTree BaseRepository::GetFeatureTreeImpl ()
{
	FeatureTree result;

	std::unordered_map < int64, std::pair<size_t, int> > featureTreeNodeToFeatures;
	auto featuresPerNodeQuery = GetDatabase ().Prepare (
		"SELECT NodeId, Uuid FROM ui_feature_tree_feature_references "
		"LEFT JOIN features ON features.Id = ui_feature_tree_feature_references.FeatureId "
		"ORDER BY NodeId;"
	);

	std::vector<Uuid> featureIds;

	///@TODO(minor) Query number of Uuids/nodes first, then allocate once and
	///bulk insert things.
	while (featuresPerNodeQuery.Step ()) {
		auto nodeId = featuresPerNodeQuery.GetInt64 (0);

		if (featureTreeNodeToFeatures.find (nodeId) == featureTreeNodeToFeatures.end ()) {
			featureTreeNodeToFeatures[nodeId] = std::make_pair (
				featureIds.size (), 0
			);
		}

		Uuid uuid;
		featuresPerNodeQuery.GetBlob (1, uuid);
		featureIds.push_back (uuid);
		featureTreeNodeToFeatures[nodeId].second += 1;
	}
	featuresPerNodeQuery.Reset ();

	auto nodePointer = result.SetFeatureIds (featureIds);

	std::unordered_map<int64, FeatureTreeNode*> idToFeatureTreeNode;

	// We need two steps, first we populate all nodes, then we link them
	auto getNodesQuery = GetDatabase ().Prepare ("SELECT Id, Name, Description "
		"FROM ui_feature_tree_nodes ORDER BY Id;");

	while (getNodesQuery.Step ()) {
		auto node = std::make_unique<FeatureTreeNode> ();
		node->name = getNodesQuery.GetText (1);
		node->description = getNodesQuery.GetText (2);

		const auto id = getNodesQuery.GetInt64 (0);
		node->featureIds = nodePointer + featureTreeNodeToFeatures[id].first;
		node->featureIdCount = featureTreeNodeToFeatures[id].second;

		idToFeatureTreeNode[id] = node.get ();

		result.nodes.emplace_back (std::move (node));
	}
	getNodesQuery.Reset ();

	auto getNodeParentQuery = GetDatabase ().Prepare ("SELECT Id, ParentId "
		"FROM ui_feature_tree_nodes WHERE ParentId NOT NULL;");

	while (getNodeParentQuery.Step ()) {
		idToFeatureTreeNode.find (getNodeParentQuery.GetInt64 (0))->second->parent =
			idToFeatureTreeNode.find (getNodeParentQuery.GetInt64 (1))->second;
	}
	getNodeParentQuery.Reset ();

	return result;
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
std::vector<Repository::Dependency> BaseRepository::GetFeatureDependenciesImpl (const Uuid& featureId)
{
	auto query = GetDatabase ().Prepare (
		"SELECT SourceUuid, TargetUuid, Relation FROM "
		"feature_dependencies_with_uuid WHERE SourceUuid=?1 OR TargetUuid=?1");

	query.BindArguments (featureId);

	std::vector<Repository::Dependency> result;
	while (query.Step ()) {
		Repository::Dependency dependency;
		query.GetBlob (0, dependency.source);
		query.GetBlob (1, dependency.target);

		result.push_back (dependency);

		///@TODO(minor) Assert all relationships are "requires"
	}

	return result;
}
}