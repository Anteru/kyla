#include <boost/program_options.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <set>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "FileIO.h"

#include "Log.h"

#include "Hash.h"
#include "SourcePackage.h"
#include "SourcePackageReader.h"

#include "Installer.h"

#include "sql/Database.h"

#undef CreateFile

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
void InstallationEnvironment::SetProperty (const PropertyCategory category,
	const std::string& name, const Property& value)
{
	switch (category) {
	case PropertyCategory::Installation:
		installationProperties_ [name] = value;
		break;

	case PropertyCategory::Internal:
		internalProperties_ [name] = value;
		break;

	// cannot set other properties
	}
}

////////////////////////////////////////////////////////////////////////////////
bool InstallationEnvironment::HasProperty (const PropertyCategory category,
	const std::string& name) const
{
	switch (category) {
	case PropertyCategory::Installation:
		return (installationProperties_.find (name) != installationProperties_.end ());

	case PropertyCategory::Internal:
		return (internalProperties_.find (name) != internalProperties_.end ());

	/// TODO Handle environment properties here

	default:
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
const Property& InstallationEnvironment::GetProperty (const PropertyCategory category,
	const std::string& name) const
{
	switch (category) {
	case PropertyCategory::Installation:
		return installationProperties_.find (name)->second;

	case PropertyCategory::Internal:
		return internalProperties_.find (name)->second;

	default:
		throw std::runtime_error ("Unsupported property");
	}
}

////////////////////////////////////////////////////////////////////////////////
template <typename T>
std::string Join (const std::vector<T>& elements, const char* infix = ", ")
{
	std::stringstream result;

	for (typename std::vector<T>::size_type i = 0, e = elements.size (); i < e; ++i) {
		result << elements [i];

		if (i+1 < e) {
			result << infix;
		}
	}

	return result.str ();
}

////////////////////////////////////////////////////////////////////////////////
std::string GetSourcePackagesForSelectedFeaturesQueryString (
	const std::vector<int>& featureIds)
{
	std::stringstream result;
	result << "SELECT Filename FROM source_packages WHERE Id IN ("
		   << "SELECT SourcePackageId FROM storage_mapping WHERE ContentObjectId "
		   << "IN (SELECT ContentObjectId FROM files WHERE FeatureId IN ("
		   << Join (featureIds)
		   << ")) GROUP BY SourcePackageId);";
	return result.str ();
}

////////////////////////////////////////////////////////////////////////////////
std::string GetContentObjectHashesChunkCountForSelectedFeaturesQueryString (
	const std::vector<int>& featureIds)
{
	std::stringstream result;
	result << "SELECT Hash, ChunkCount, Size, LENGTH(Hash) FROM content_objects WHERE Id IN ("
		   << "SELECT ContentObjectId FROM files WHERE FeatureId IN ("
		   << Join (featureIds)
			  // We have to group by to resolve duplicates
		   << ") GROUP BY ContentObjectId);";
	return result.str ();
}

////////////////////////////////////////////////////////////////////////////////
std::string GetFilesForSelectedFeaturesQueryString (
	const std::vector<int>& featureIds)
{
	std::stringstream result;
	result << "SELECT Path, Hash, LENGTH(Hash) FROM files JOIN content_objects "
		   << "ON files.ContentObjectId = content_objects.Id WHERE FeatureId IN ("
		   << Join (featureIds)
		   << ");";
	return result.str ();
}

////////////////////////////////////////////////////////////////////////////////
void Installer::InstallProduct (Sql::Database& db, InstallationEnvironment env,
	const std::vector<int>& selectedFeatureIds)
{
	const char* logFilename = nullptr;
	if (env.HasProperty (PropertyCategory::Internal, "LogFilename")) {
		logFilename = env.GetProperty (PropertyCategory::Internal, "LogFilename").GetString ();
	}

	LogLevel logLevel = LogLevel::Info;
	if (env.HasProperty (PropertyCategory::Internal, "LogLevel")) {
		logLevel = static_cast<LogLevel> (
			env.GetProperty (PropertyCategory::Internal, "LogLevel").GetInt ());
	}

	Log log {"Install", logFilename, logLevel};

	const auto sourcePackageDirectory = env.HasProperty (
		PropertyCategory::Installation, "SourcePackageDirectory") ?
			absolute (boost::filesystem::path (
				env.GetProperty (PropertyCategory::Installation, "SourcePackageDirectory").GetString ()))
		:	absolute (boost::filesystem::path ("."));

	const boost::filesystem::path targetDirectory =
		env.GetProperty (PropertyCategory::Installation, "TargetDirectory").GetString ();
	const auto stagingDirectory = env.HasProperty (PropertyCategory::Installation, "StagingDirectory") ?
			absolute (boost::filesystem::path (
				env.GetProperty (PropertyCategory::Installation, "StagingDirectory").GetString ()))
		:	absolute (boost::filesystem::path ("./stage"));

	boost::filesystem::create_directories (targetDirectory);
	boost::filesystem::create_directories (stagingDirectory);

	auto selectRequiredSourcePackagesStatement  = db.Prepare (
		GetSourcePackagesForSelectedFeaturesQueryString (selectedFeatureIds));

	std::vector<std::string> requiredSourcePackageFilenames;
	while (selectRequiredSourcePackagesStatement.Step ()) {
		const std::string packageFilename =
			selectRequiredSourcePackagesStatement.GetText (0);
		log.Debug () << "Requesting package " << packageFilename;
		requiredSourcePackageFilenames.push_back (packageFilename);
	}

	auto selectRequiredContentObjectsStatement = db.Prepare (
		GetContentObjectHashesChunkCountForSelectedFeaturesQueryString (selectedFeatureIds));

	std::unordered_map<kyla::SHA512Digest, int, kyla::HashDigestHash, kyla::HashDigestEqual> requiredContentObjects;
	while (selectRequiredContentObjectsStatement.Step ()) {
		kyla::SHA512Digest digest;

		const auto digestSize = selectRequiredContentObjectsStatement.GetInt64 (3);

		if (digestSize != sizeof (digest.bytes)) {
			log.Error () << "Hash digest size mismatch, skipping content object";
			continue;
		}

		::memcpy (digest.bytes,
			selectRequiredContentObjectsStatement.GetBlob (0),
			sizeof (digest.bytes));

		const auto chunkCount = selectRequiredContentObjectsStatement.GetInt64 (1);
		const auto size = selectRequiredContentObjectsStatement.GetInt64 (2);
		requiredContentObjects [digest] = chunkCount;

		kyla::CreateFile (
			(stagingDirectory / ToString (digest)).string ().c_str ())->SetSize (size);

		log.Trace () << "Content object " << ToString (digest) << " allocated (" << size << " bytes)";
	}

	log.Info () << "Requested " << requiredContentObjects.size () << " content objects";

	// Process all source packages into the staging directory, only extracting
	// the requested content objects
	// As we have pre-allocated everything, this can run in parallel
	for (const auto& sourcePackageFilename : requiredSourcePackageFilenames) {
		kyla::FileSourcePackageReader reader (sourcePackageDirectory / sourcePackageFilename);

		log.Info () << "Processing source package " << sourcePackageFilename;

		reader.Store ([&requiredContentObjects](const kyla::SHA512Digest& digest) -> bool {
			return requiredContentObjects.find (digest) != requiredContentObjects.end ();
		}, stagingDirectory, log);

		log.Info () << "Processed source package " << sourcePackageFilename;
	}

	// Once done, we walk once more over the file list and just copy the
	// content object to its target location
	auto selectFilesStatement = db.Prepare (
		GetFilesForSelectedFeaturesQueryString (selectedFeatureIds));

	// Find unique directory paths first
	std::set<std::string> directories;

	while (selectFilesStatement.Step ()) {
		const auto targetPath =
			targetDirectory / selectFilesStatement.GetText (0);

		directories.insert (targetPath.parent_path ().string ());
	}

	// This is sorted by length, so child paths always come after parent paths
	for (const auto directory : directories) {
		if (! boost::filesystem::exists (directory)) {
			boost::filesystem::create_directories (directory);

			log.Debug () << "Creating directory " << directory;
		}
	}

	// Run again now that the directories have been created
	selectFilesStatement.Reset ();

	log.Info () << "Created directories";
	log.Info () << "Deploying files";

	while (selectFilesStatement.Step ()) {
		const auto targetPath =
			targetDirectory / selectFilesStatement.GetText (0);

		// If null, we need to create an empty file there
		if (selectFilesStatement.GetColumnType (1) == Sql::Type::Null) {
			log.Debug () << "Creating empty file " << targetPath.string ();

			kyla::CreateFile (targetPath.string ().c_str ());
		} else {
			kyla::SHA512Digest digest;

			const auto digestSize = selectFilesStatement.GetInt64 (2);

			if (digestSize != sizeof (digest.bytes)) {
				log.Error () << "Hash size mismatch, skipping file";
				continue;
			}

			::memcpy (digest.bytes, selectFilesStatement.GetBlob (1),
				sizeof (digest.bytes));

			log.Debug () << "Copying " << (stagingDirectory / ToString (digest)).string ()
						 << " to " << absolute (targetPath).string ();

			// Make this smarter, i.e. move first time, and on second time, copy
			boost::filesystem::copy_file (stagingDirectory / ToString (digest),
				targetPath);
		}
	}

	log.Info () << "Done";
}
}
