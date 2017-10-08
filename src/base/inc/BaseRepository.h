/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_BASE_REPOSITORY_H
#define KYLA_CORE_INTERNAL_BASE_REPOSITORY_H

#include "Repository.h"
#include <string>

namespace kyla {
class BaseRepository : public Repository
{
public:
	virtual ~BaseRepository () = default;

private:
	virtual std::vector<Uuid> GetFeaturesImpl () override;
	virtual int64_t GetFeatureSizeImpl (const Uuid& featureId) override;
	virtual int64_t GetFeatureInstallationLevel (const Uuid& featureId);
	virtual bool IsEncryptedImpl () override;
	virtual void SetDecryptionKeyImpl (const std::string& key) override;
	virtual std::string GetDecryptionKeyImpl () const override;

	void RepairImpl (Repository& source,
		ExecutionContext& context,
		RepairCallback repairCallback,
		bool restore) override;
	void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& featureIds,
		ExecutionContext& context) override;
	std::vector<Dependency> GetFeatureDependenciesImpl (const Uuid& featureId) override;

	FeatureTree GetFeatureTreeImpl () override;

protected:
	std::string key_;

	static constexpr int FeatureInstallationLevelPropertyId = 1;
};
}

#endif
