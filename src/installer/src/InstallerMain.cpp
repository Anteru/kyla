#include <sqlite3.h>
#include <openssl/evp.h>
#include <boost/program_options.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <set>

#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

// For Linux memory mapping
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Hash.h"
#include "SourcePackage.h"

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
	const std::vector<std::int64_t>& featureIds)
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
	const std::vector<std::int64_t>& featureIds)
{
	std::stringstream result;
	result << "SELECT Hash, ChunkCount, Size FROM content_objects WHERE Id IN ("
		   << "SELECT ContentObjectId FROM files WHERE FeatureId IN ("
		   << Join (featureIds)
			  // We have to group by to resolve duplicates
		   << ") GROUP BY ContentObjectId);";
	return result.str ();
}

////////////////////////////////////////////////////////////////////////////////
std::string GetFilesForSelectedFeaturesQueryString (
	const std::vector<std::int64_t>& featureIds)
{
	std::stringstream result;
	result << "SELECT Path, Hash FROM files JOIN content_objects "
		   << "ON files.ContentObjectId = content_objects.Id WHERE FeatureId IN ("
		   << Join (featureIds)
		   << ");";
	return result.str ();
}

////////////////////////////////////////////////////////////////////////////////
int main (int argc, char* argv [])
{
	namespace po = boost::program_options;
	po::options_description generic ("Generic options");
	generic.add_options ()
		("help,h", "Show help message");

	po::options_description desc ("Configuration");
	desc.add_options ()
		("package-directory", po::value<std::string> ()->default_value ("."));

	po::options_description hidden ("Hidden options");
	hidden.add_options ()
		("input-file", po::value<std::string> ());

	po::options_description cmdline_options;
	cmdline_options.add (generic).add (desc).add (hidden);

	po::options_description visible_options;
	visible_options.add (generic).add (desc);

	po::positional_options_description p;
	p.add("input-file", 1);

	po::variables_map vm;
	po::store (po::command_line_parser (argc, argv)
		.options (cmdline_options).positional (p).run (), vm);
	po::notify (vm);

	if (vm.count ("help")) {
		std::cout << visible_options << std::endl;
		return 0;
	}

	const auto inputFilePath =
		absolute (boost::filesystem::path (vm ["input-file"].as<std::string> ()));
	const auto packageDirectory = absolute (boost::filesystem::path (
		vm ["package-directory"].as<std::string> ()));

	boost::filesystem::path installationDirectory = "install";
	boost::filesystem::path stagingDirectory = "stage";
	boost::filesystem::create_directories (installationDirectory);
	boost::filesystem::create_directories (stagingDirectory);

	sqlite3* db;
	sqlite3_open_v2 (inputFilePath.c_str (), &db,
		SQLITE_OPEN_READONLY, nullptr);

	sqlite3_stmt* selectFeaturesStatement;
	sqlite3_prepare_v2 (db,
		"SELECT Id, Name, UIName FROM features;", -1,
		&selectFeaturesStatement, nullptr);

	std::vector<std::int64_t> selectedFeatureIds;

	std::cout << "Installing features:\n";
	while (sqlite3_step (selectFeaturesStatement) == SQLITE_ROW) {
		const std::string featureName = reinterpret_cast<const char*> (
			sqlite3_column_text (selectFeaturesStatement, 2));
		std::cout << "\t" << featureName << "\n";

		selectedFeatureIds.push_back (
			sqlite3_column_int64 (selectFeaturesStatement, 0));
	}

	sqlite3_finalize (selectFeaturesStatement);

	sqlite3_stmt* selectRequiredSourcePackagesStatement = nullptr;
	sqlite3_prepare_v2 (db,
		GetSourcePackagesForSelectedFeaturesQueryString (selectedFeatureIds).c_str (),
		-1, &selectRequiredSourcePackagesStatement, nullptr);

	std::vector<std::string> requiredSourcePackageFilenames;
	while (sqlite3_step (selectRequiredSourcePackagesStatement) == SQLITE_ROW) {
		const std::string packageFilename =
			reinterpret_cast<const char*> (sqlite3_column_text (selectRequiredSourcePackagesStatement, 0));
		BOOST_LOG_TRIVIAL (debug) << "Requesting package " << packageFilename;
		requiredSourcePackageFilenames.push_back (packageFilename);
	}

	sqlite3_finalize (selectRequiredSourcePackagesStatement);

	sqlite3_stmt* selectRequiredContentObjectsStatement = nullptr;
	sqlite3_prepare_v2 (db,
		GetContentObjectHashesChunkCountForSelectedFeaturesQueryString (selectedFeatureIds).c_str (),
		-1, &selectRequiredContentObjectsStatement, nullptr);

	std::unordered_map<Hash, int, HashHash, HashEqual> requiredContentObjects;
	while (sqlite3_step (selectRequiredContentObjectsStatement) == SQLITE_ROW) {
		Hash hash;
		::memcpy (hash.hash,
			sqlite3_column_blob (selectRequiredContentObjectsStatement, 0),
			sizeof (hash.hash));

		int chunkCount = sqlite3_column_int (selectRequiredContentObjectsStatement, 1);
		const auto size = sqlite3_column_int64 (selectRequiredContentObjectsStatement, 2);
		requiredContentObjects [hash] = chunkCount;

		// Linux-specific
		auto fd = open ((stagingDirectory / ToString (hash)).c_str (),
			 O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

		if (ftruncate(fd, size) == 0) {
			BOOST_LOG_TRIVIAL(trace) << "Content object " << ToString (hash) << " allocated";
		} else {
			BOOST_LOG_TRIVIAL(error) << "Could not allocate content object " << ToString (hash);
		}

		close (fd);
	}

	BOOST_LOG_TRIVIAL (debug) << "Requested " << requiredContentObjects.size () << " content objects";

	sqlite3_finalize (selectRequiredContentObjectsStatement);

	// Process all source packages into temporary directory, only extracting
	// the requested content objects
	// As we have pre-allocated everything, this can run in parallel
	for (const auto& sourcePackageFilename : requiredSourcePackageFilenames) {
		FileSourcePackageReader reader (sourcePackageFilename);

		reader.Store ([&requiredContentObjects](const Hash& hash) -> bool {
			return requiredContentObjects.find (hash) != requiredContentObjects.end ();
		}, stagingDirectory);
	}

	// Once done, we walk once more over the file list and just copy the
	// content object to its target location
	sqlite3_stmt* selectFilesStatement = nullptr;
	sqlite3_prepare_v2(db,
		GetFilesForSelectedFeaturesQueryString (selectedFeatureIds).c_str (),
		-1, &selectFilesStatement, nullptr);

	// Find unique directory paths first
	std::set<std::string> directories;

	while (sqlite3_step (selectFilesStatement) == SQLITE_ROW) {
		const auto targetPath =
			installationDirectory / (reinterpret_cast<const char*> (
				sqlite3_column_text (selectFilesStatement, 0)));

		directories.insert (targetPath.parent_path ().string ());
	}

	// This is sorted by length, so child paths always come after parent paths
	for (const auto directory : directories) {
		if (! boost::filesystem::exists (directory)) {
			boost::filesystem::create_directory (directory);

			BOOST_LOG_TRIVIAL(debug) << "Creating directory " << directory;
		}
	}

	sqlite3_reset (selectFilesStatement);

	while (sqlite3_step (selectFilesStatement) == SQLITE_ROW) {
		const auto targetPath =
			installationDirectory / (reinterpret_cast<const char*> (
				sqlite3_column_text (selectFilesStatement, 0)));

		// If null, we need to create an empty file there
		if (sqlite3_column_type (selectFilesStatement, 1) == SQLITE_NULL) {
			boost::filesystem::ofstream output (targetPath, std::ios::binary);
			output.close ();
		} else {
			Hash hash;
			// TODO Validate size
			::memcpy (hash.hash, sqlite3_column_blob (selectFilesStatement, 1),
				sizeof (hash.hash));

			BOOST_LOG_TRIVIAL(debug) << "Copying " << (stagingDirectory / ToString (hash)) << " to " << targetPath;

			// Make this smarter, i.e. move first time, and on second time, copy
			boost::filesystem::copy_file (stagingDirectory / ToString (hash),
				targetPath);
		}
	}

	sqlite3_finalize (selectFilesStatement);

	sqlite3_close (db);
}
