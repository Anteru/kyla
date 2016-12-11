/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "Repository.h"

#include "DeployedRepository.h"
#include "LooseRepository.h"
#include "PackedRepository.h"
#include "WebRepository.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void Repository::Validate (const ValidationCallback& validationCallback,
	ExecutionContext& context)
{
	ValidateImpl (validationCallback, context);
}

///////////////////////////////////////////////////////////////////////////////
void Repository::Repair (Repository& source, ExecutionContext& context)
{
	RepairImpl (source, context);
}

///////////////////////////////////////////////////////////////////////////////
void Repository::Configure (Repository& source, const ArrayRef<Uuid>& features,
	ExecutionContext& context)
{
	ConfigureImpl (source, features, context);
}

///////////////////////////////////////////////////////////////////////////////
void Repository::GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
	const GetContentObjectCallback& getCallback)
{
	GetContentObjectsImpl (requestedObjects, getCallback);
}

///////////////////////////////////////////////////////////////////////////////
std::vector<Uuid> Repository::GetFeatures ()
{
	return GetFeaturesImpl ();
}

///////////////////////////////////////////////////////////////////////////////
int64_t Repository::GetFeatureSize (const Uuid& id)
{
	return GetFeatureSizeImpl (id);
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& Repository::GetDatabase ()
{
	return GetDatabaseImpl ();
}

///////////////////////////////////////////////////////////////////////////////
bool Repository::IsEncrypted ()
{
	return IsEncryptedImpl ();
}

///////////////////////////////////////////////////////////////////////////////
void Repository::SetDecryptionKey (const std::string& key)
{
	SetDecryptionKeyImpl (key);
}

///////////////////////////////////////////////////////////////////////////////
std::string Repository::GetDecryptionKey () const
{
	return GetDecryptionKeyImpl ();
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<Repository> OpenRepository (const char* path,
	const bool allowWrite)
{
	///@TODO(minor) Move this logic into a static member function of the
	/// various repository types
	if (strncmp (path, "http", 4) == 0) {
		return std::unique_ptr<Repository> (new WebRepository{ path });
	} else if (boost::filesystem::exists (Path{ path } / Path{ ".ky" })) {
		// .ky indicates a loose repository
		return std::unique_ptr<Repository> (new LooseRepository{ path });
	} else if (boost::filesystem::exists (Path{ path } / "repository.db")) {
		return std::unique_ptr<Repository> (new PackedRepository{ path });
	}  else {
		// Assume deployed repository for now
		return std::unique_ptr<Repository> (new DeployedRepository{ path,
			allowWrite ? Sql::OpenMode::ReadWrite : Sql::OpenMode::Read });
	}
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<Repository> DeployRepository (Repository& source,
	const char* destinationPath,
	const ArrayRef<Uuid>& features,
	Repository::ExecutionContext& context)
{
	Path targetPath{ destinationPath };
	boost::filesystem::create_directories (destinationPath);

	return std::unique_ptr<Repository> (DeployedRepository::CreateFrom (source, features, targetPath, 
		context).release ());
}
}
