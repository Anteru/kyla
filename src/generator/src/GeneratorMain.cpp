#include "SourcePackage.h"
#include "Hash.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <iostream>
#include <fstream>
#include <string>

#include <pugixml.hpp>
#include <sqlite3.h>

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <boost/log/trivial.hpp>
#include <openssl/evp.h>
#include <zlib.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "build-db-structure.h"
#include "install-db-structure.h"

std::unordered_set<std::string> GetUniqueSourcePaths (const pugi::xml_node& product)
{
	std::unordered_set<std::string> result;

	int inputFileCount = 0;
	for (auto files : product.children ("Files")) {
		for (auto file : files.children ("File")) {
			// TODO Also validate that these are actually file paths
			// TODO Validate that a source path is not empty
			// TODO Validate that the target path is not empty if present
			// TODO Validate that target paths don't collide

			result.insert (file.attribute ("Source").value ());

			++inputFileCount;
		}
	}

	BOOST_LOG_TRIVIAL(info) << "Processed " << inputFileCount
		<< " source paths, " << result.size () << " unique paths found.";

	return result;
}

/**
For every unique source file, create the file chunks and update the build
database accordingly.

After this function has run:
  - Every file in the input set has been hash'ed, compressed and chunked
  - All chunks are stored in the temporary directory
  - The buildDatabase has been populated (content_objects and chunks)
*/
void PrepareFiles (
	const std::unordered_set<std::string>& sourcePaths,
	const boost::filesystem::path& sourceDirectory,
	const boost::filesystem::path& temporaryDirectory,
	sqlite3* buildDatabase,
	const std::int64_t fileChunkSize = 1 << 24 /* 16 MiB */)
{
	// Read every unique source path in chunks, update hash, compress, compute
	// hash, write to temporary directory
	std::vector<unsigned char> buffer (fileChunkSize);
	std::vector<unsigned char> compressed (compressBound(fileChunkSize));

	// Prepare hash
	EVP_MD_CTX* fileCtx = EVP_MD_CTX_create ();
	EVP_MD_CTX* chunkCtx = EVP_MD_CTX_create ();

	boost::uuids::random_generator uuidGen;

	sqlite3_stmt* insertContentObjectStatement;
	sqlite3_prepare (buildDatabase,
		"INSERT INTO content_objects (Size, Hash) VALUES (?, ?)",
		-1, &insertContentObjectStatement, nullptr);

	sqlite3_stmt* insertChunkStatement;
	sqlite3_prepare (buildDatabase,
		"INSERT INTO chunks (ContentObjectId, Path) VALUES (?, ?)",
		-1, &insertChunkStatement, nullptr);

	for (const auto sourcePath : sourcePaths) {
		const auto fullSourcePath = sourceDirectory / sourcePath;

		// TODO handle read errors
		// TODO handle empty files
		boost::filesystem::ifstream input (fullSourcePath, std::ios::binary);

		std::vector<std::string> chunks;

		EVP_DigestInit_ex (fileCtx, EVP_sha512 (), nullptr);
		int chunkNumber = 0;
		std::int64_t contentObjectSize = 0;

		for (;;) {
			input.read (reinterpret_cast<char*> (buffer.data ()), fileChunkSize);
			const std::int64_t chunkSize = input.gcount ();
			contentObjectSize += chunkSize;

			EVP_DigestUpdate(fileCtx, buffer.data (), chunkSize);

			uLongf compressedSize = compressed.size ();
			// TODO handle compression failure
			compress2 (reinterpret_cast<Bytef*> (compressed.data ()),
				&compressedSize,
				buffer.data (), chunkSize, Z_BEST_COMPRESSION);

			const auto chunkName = ToString (uuidGen ().data);
			chunks.push_back (chunkName);

			PackageDataChunk pdc;
			::memset (&pdc, 0, sizeof (pdc));

			pdc.compressedSize = compressedSize;
			pdc.size = chunkSize;
			pdc.offset = fileChunkSize * chunkNumber;
			pdc.compressionMode = CompressionMode_Zip;

			EVP_DigestInit_ex (chunkCtx, EVP_sha512 (), nullptr);
			EVP_DigestUpdate(chunkCtx, compressed.data (), compressedSize);
			EVP_DigestFinal_ex (chunkCtx, pdc.hash, nullptr);

			boost::filesystem::ofstream chunkOutput (
				temporaryDirectory / chunkName, std::ios::binary);
			chunkOutput.write (reinterpret_cast<const char*> (&pdc), sizeof (pdc));
			chunkOutput.write (reinterpret_cast<char*> (compressed.data ()),
				compressedSize);

			BOOST_LOG_TRIVIAL(debug) << "Wrote chunk '" << chunkName
				<< "' for file '" << sourcePath << "' (uncompressed: "
				<< chunkSize << ", compressed: " << compressedSize << ")";

			++chunkNumber;

			// Compress data
			if (input.gcount () < fileChunkSize) {
				break;
			}
		}

		Hash fileHash;
		EVP_DigestFinal_ex (fileCtx, fileHash.hash, nullptr);

		sqlite3_bind_int64 (insertContentObjectStatement, 1, contentObjectSize);
		sqlite3_bind_text (insertContentObjectStatement, 2,
			ToString (fileHash).c_str (), -1, nullptr);
		sqlite3_step (insertContentObjectStatement);
		sqlite3_reset (insertContentObjectStatement);

		const auto contentObjectId = sqlite3_last_insert_rowid (buildDatabase);

		for (const auto chunk : chunks) {
			sqlite3_bind_int64 (insertChunkStatement, 1, contentObjectId);
			sqlite3_bind_text (insertChunkStatement, 2,
				absolute (temporaryDirectory / chunk).c_str (), -1, nullptr);
			sqlite3_step (insertChunkStatement);
			sqlite3_reset (insertChunkStatement);
		}
	}

	EVP_MD_CTX_destroy (fileCtx);
	EVP_MD_CTX_destroy (chunkCtx);
}

////////////////////////////////////////////////////////////////////////////////
void PackPackages (sqlite3* installationDatabase, sqlite3* buildDatabase,
	const boost::filesystem::path& targetDirectory,
	const pugi::xml_node& productNode,
	const std::int64_t targetPackageSize = 1ll << 30 /* 1 GiB */)
{
	// Each <Files> group goes into either one package or the default package
	// for the default packages, we start a new one once the targetPackageSize
	// has been reached
}

struct GeneratorContext
{
public:
	GeneratorContext ()
	{
		sqlite3_open (":memory:", &installationDatabase);

		// private, temporary, but disk-based
		sqlite3_open ("", &buildDatabase);

		sqlite3_exec (installationDatabase, install_db_structure,
			nullptr, nullptr, nullptr);
		sqlite3_exec (buildDatabase, build_db_structure,
			nullptr, nullptr, nullptr);
	}

	~GeneratorContext ()
	{
		sqlite3_close (buildDatabase);
		sqlite3_close (installationDatabase);
	}

	bool WritePackage (const std::string& filename) const
	{

	}

	pugi::xml_node	productNode;
	sqlite3*		installationDatabase;
	sqlite3*		buildDatabase;
	boost::filesystem::path sourceDirectory;
	boost::filesystem::path	temporaryDirectory;
};

/*
Returns a mapping of feature id string to the feature id in the database.
*/
std::unordered_map<std::string, std::int64_t> CreateFeatures (GeneratorContext& gc)
{
	// TODO Validate that there is a feature element
	// TODO Validate that at least one feature is present
	// TODO Validate the feature ID is not empty
	// TODO Validate all feature IDs are unique
	BOOST_LOG_TRIVIAL(info) << "Populating feature table";

	std::unordered_map<std::string, std::int64_t> result;

	sqlite3_stmt* insertFeatureStatement;
	sqlite3_prepare_v2 (gc.installationDatabase,
		"INSERT INTO features (Name) VALUES (?)", -1, &insertFeatureStatement,
		nullptr);

	int featureCount = 0;
	for (const auto feature : gc.productNode.child ("Features").children ()) {
		const auto featureId = feature.attribute ("Id").value ();
		sqlite3_bind_text (insertFeatureStatement, 1, featureId, -1, nullptr);
		sqlite3_step (insertFeatureStatement);
		sqlite3_reset (insertFeatureStatement);

		const auto lastRowId = sqlite3_last_insert_rowid (gc.installationDatabase);
		result [featureId] = lastRowId;

		BOOST_LOG_TRIVIAL(debug) << "Feature '" << featureId
			<< "' assigned to Id " << lastRowId;
		++featureCount;
	}

	sqlite3_finalize (insertFeatureStatement);

	BOOST_LOG_TRIVIAL(info) << "Created " << featureCount << " feature(s)";

	return result;
}

void CreateSourcePackages (GeneratorContext& gc)
{
	// First, we iterate over all files, and put them right into their packages

	// We can only populate the database once we know all packages
}

int main (int argc, char* argv[])
{
    namespace po = boost::program_options;
    po::options_description generic ("Generic options");
    generic.add_options ()
        ("help,h", "Show help message");

    po::options_description desc ("Configuration");
    desc.add_options ()
		("source-directory", po::value<std::string> ()->default_value ("."))
		("temp-directory", po::value<std::string> ()->default_value (
			 boost::filesystem::unique_path ().string ()))
		("output,o", "Output file name");

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

	const auto inputFile = vm ["input-file"].as<std::string> ();

	pugi::xml_document doc;
	doc.load_file (inputFile.c_str ());

	boost::filesystem::path sourcePath (inputFile);
	const auto outputFile = sourcePath.stem ();

	GeneratorContext gc;
	gc.productNode = doc.document_element ().child ("Product");
	gc.sourceDirectory = boost::filesystem::path (
		vm ["source-directory"].as<std::string> ());
	gc.temporaryDirectory = boost::filesystem::path (
		vm ["temp-directory"].as<std::string> ());

	boost::filesystem::create_directories (gc.temporaryDirectory);
	BOOST_LOG_TRIVIAL(debug) << "Temporary directory: " << gc.temporaryDirectory;

	CreateFeatures (gc);
	const auto uniqueFiles = GetUniqueSourcePaths (gc.productNode);
	PrepareFiles (uniqueFiles, gc.sourceDirectory, gc.temporaryDirectory,
		gc.buildDatabase);

	CreateSourcePackages (gc);
	gc.WritePackage (outputFile.string () + ".nimdb");

	boost::filesystem::remove_all (gc.temporaryDirectory);
}
