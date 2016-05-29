/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

#include "Repository.h"

#include "DeployedRepository.h"
#include "LooseRepository.h"
#include "PackedRepository.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void Repository::Validate (const ValidationCallback& validationCallback)
{
	ValidateImpl (validationCallback);
}

///////////////////////////////////////////////////////////////////////////////
void Repository::Repair (Repository& source)
{
	RepairImpl (source);
}

///////////////////////////////////////////////////////////////////////////////
void Repository::Configure (Repository& source, const ArrayRef<Uuid>& filesets,
	Log& log, Progress& progress)
{
	ConfigureImpl (source, filesets, log, progress);
}

///////////////////////////////////////////////////////////////////////////////
void Repository::GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
	const GetContentObjectCallback& getCallback)
{
	GetContentObjectsImpl (requestedObjects, getCallback);
}

///////////////////////////////////////////////////////////////////////////////
std::vector<FilesetInfo> Repository::GetFilesetInfos ()
{
	return GetFilesetInfosImpl ();
}

///////////////////////////////////////////////////////////////////////////////
std::string Repository::GetFilesetName (const Uuid& id)
{
	return GetFilesetNameImpl (id);
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& Repository::GetDatabase ()
{
	return GetDatabaseImpl ();
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<Repository> OpenRepository (const char* path,
	const bool allowWrite)
{
	///@TODO(minor) Move this logic into a static member function of the
	/// various repository types
	if (boost::filesystem::exists (Path{ path } / Path{ ".ky" })) {
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
	const ArrayRef<Uuid>& filesets,
	Log& log, Progress& progress)
{
	Path targetPath{ destinationPath };
	boost::filesystem::create_directories (destinationPath);

	return std::unique_ptr<Repository> (DeployedRepository::CreateFrom (source, filesets, targetPath, 
		log, progress).release ());
}
}
