#include <sqlite3.h>
#include <openssl/evp.h>
#include <boost/program_options.hpp>
#include <string>
#include <iostream>

#include "Hash.h"

int main (int argc, char* argv[])
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

	sqlite3* installationDb;
	sqlite3_open_v2 (inputFilePath.c_str (), &installationDb,
		SQLITE_OPEN_READONLY, nullptr);

	sqlite3_stmt* selectSourcePackageCountStatement;
	sqlite3_prepare_v2 (installationDb,
		"SELECT COUNT() FROM source_packages;", -1,
		&selectSourcePackageCountStatement, nullptr);

	sqlite3_step (selectSourcePackageCountStatement);
	const auto sourcePackageCount = sqlite3_column_int64(
		selectSourcePackageCountStatement, 0);

	sqlite3_finalize (selectSourcePackageCountStatement);

	sqlite3_stmt* selectSourcePackagesStatement;
	sqlite3_prepare_v2 (installationDb,
		"SELECT Hash, Filename, LENGTH(Hash) AS HashSize FROM source_packages;", -1,
		&selectSourcePackagesStatement, nullptr);

	std::cout << "Validating " << sourcePackageCount << " source packages\n";

	std::int64_t i = 1;
	while (sqlite3_step (selectSourcePackagesStatement) == SQLITE_ROW) {
		const auto packagePath = packageDirectory /
				boost::filesystem::path (reinterpret_cast<const char*> (
					sqlite3_column_text (
						selectSourcePackagesStatement, 1)));
		std::cout << "\t"
				  << packagePath.filename () << " (" << i++ << "/"
				  << sourcePackageCount << "): ";

		Hash expectedHash;
		const auto hashSize = sqlite3_column_int64 (
			selectSourcePackagesStatement, 2);

		if (hashSize != sizeof (expectedHash.hash)) {
			std::cout << "ERROR, hash size mismatch";
			continue;
		}

		::memcpy (expectedHash.hash,
			sqlite3_column_blob (selectSourcePackagesStatement, 0),
			sizeof (expectedHash.hash));

		Hash actualHash = ComputeHash (packagePath);
		if (! HashEqual() (expectedHash, actualHash)) {
			std::cout << "ERROR, hash mismatch\n";
		} else {
			std::cout << "OK\n";
		}
	}

	sqlite3_finalize (selectSourcePackagesStatement);
	sqlite3_close (installationDb);
}
