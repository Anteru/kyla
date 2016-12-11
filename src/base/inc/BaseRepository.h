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
	virtual bool IsEncryptedImpl () override;
	virtual void SetDecryptionKeyImpl (const std::string& key) override;
	virtual std::string GetDecryptionKeyImpl () const override;

	void RepairImpl (Repository& source, ExecutionContext& context) override;
	void ValidateImpl (const ValidationCallback& validationCallback,
		ExecutionContext& context) override;
	void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& featureIds,
		ExecutionContext& context) override;

protected:
	std::string key_;
};
}

#endif
