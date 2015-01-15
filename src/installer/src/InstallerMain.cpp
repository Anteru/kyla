#include <sqlite3.h>
#include <openssl/evp.h>
#include <boost/program_options.hpp>
#include <string>

#include "Hash.h"

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

	sqlite3* db;
	sqlite3_open_v2 (inputFilePath.c_str (), &db,
		SQLITE_OPEN_READONLY, nullptr);

	sqlite3_stmt* selectFeaturesStatement;
	sqlite3_prepare_v2 (db,
		"SELECT Id, Name, UIName FROM features;", -1,
		&selectFeaturesStatement, nullptr);

	std::cout << "Installing features:\n";
	while (sqlite3_step (selectFeaturesStatement) == SQLITE_ROW) {
		const std::string featureName = reinterpret_cast<const char*> (
			sqlite3_column_text (selectFeaturesStatement, 2));
		std::cout << "\t" << featureName << "\n";
	}

	sqlite3_finalize (selectFeaturesStatement);

	sqlite3_close (db);
}
