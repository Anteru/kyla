#include <boost/program_options.hpp>

#include "Kyla.h"

#include <iostream>

///////////////////////////////////////////////////////////////////////////////
int main (int argc, char* argv [])
{
	namespace po = boost::program_options;
	po::options_description global ("Global options");
	global.add_options ()
		("command", po::value<std::string> (), "command to execute")
		("subargs", po::value<std::vector<std::string> > (), "Arguments for command");

	po::positional_options_description pos;
	pos.add ("command", 1).
		add ("subargs", -1);

	po::variables_map vm;

	po::parsed_options parsed = po::command_line_parser (argc, argv)
		.options (global)
		.positional (pos)
		.allow_unregistered ()
		.run ();

	po::store (parsed, vm);

	if (vm.find ("command") == vm.end ()) {
		global.print (std::cout);
		return 0;
	}

	const auto cmd = vm ["command"].as<std::string> ();

	auto options = po::collect_unrecognized (parsed.options, po::include_positional);
	// Remove the command name
	options.erase (options.begin ());

	if (cmd == "build") {
		po::options_description build_desc ("build options");
		build_desc.add_options ()
			("source-directory", po::value<std::string> ()->default_value ("."),
				"Source directory")
			("input", po::value<std::string> ())
			("output-directory", po::value<std::string> ());

		po::positional_options_description posBuild;
		posBuild
			.add ("input", 1)
			.add ("output-directory", 1);

		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);

		kylaBuildEnvironment buildEnv;
		buildEnv.targetDirectory = vm ["output-directory"].as<std::string> ().c_str ();
		buildEnv.sourceDirectory = vm ["source-directory"].as<std::string> ().c_str ();

		kylaBuildRepository (vm ["input"].as<std::string> ().c_str (),
			&buildEnv);
	} else if (cmd == "validate") {
		po::options_description build_desc ("validation options");
		build_desc.add_options ()
			("input", po::value<std::string> ());
		
		po::positional_options_description posBuild;
		posBuild
			.add ("input", 1);

		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);

		auto validationCallback = [](const char* file, int validationResult,
			void*) -> void {
			switch (validationResult) {
			case kylaValidationResult_Ok:
				std::cout << "OK:        " << file << '\n';
				break;

			case kylaValidationResult_Missing:
				std::cout << "MISSING:   " << file << '\n';
				break;

			case kylaValidationResult_Corrupted:
				std::cout << "CORRUPTED: " << file << '\n';
				break;
			}
		};

		kylaValidateRepository (vm ["input"].as<std::string> ().c_str (),
			validationCallback, nullptr);
	}
}
