#include <boost/program_options.hpp>

#include "Kyla.h"

#include <iostream>
#include "Uuid.h"

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
			("verbose,v", po::bool_switch ()->default_value (false),
				"verbose output")
			("summary,s", po::bool_switch ()->default_value (true),
				"show summary")
			("input", po::value<std::string> ());
		
		po::positional_options_description posBuild;
		posBuild
			.add ("input", 1);

		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);

		int errors = 0;
		int ok = 0;

		struct Context
		{
			int* errors;
			int* ok;
			bool verbose;
		};

		Context context = { &errors, &ok, vm["verbose"].as<bool> () };

		auto validationCallback = [](int validationResult,
			const kylaValidationItemInfo* info,
			void* pContext) -> void {
			auto context = static_cast<Context*> (pContext);

			switch (validationResult) {
			case kylaValidationResult_Ok:
				if (context->verbose) {
					std::cout << "OK        " << info->filename << '\n';
				}
				++(*context->ok);
				break;

			case kylaValidationResult_Missing:
				if (context->verbose) {
					std::cout << "MISSING   " << info->filename << '\n';
				}
				++(*context->errors);
				break;

			case kylaValidationResult_Corrupted:
				if (context->verbose) {
					std::cout << "CORRUPTED " << info->filename << '\n';
				}
				++(*context->errors);
				break;
			}
		};

		kylaValidateRepository (vm ["input"].as<std::string> ().c_str (),
			validationCallback, &context);

		if (vm ["summary"].as<bool> ()) {
			std::cout << "OK " << ok << " CORRUPTED/MISSING " << errors << std::endl;
		}
	} else if (cmd == "repair") {
		po::options_description build_desc ("repair options");
		build_desc.add_options ()
			("source", po::value<std::string> ())
			("target", po::value<std::string> ());

		po::positional_options_description posBuild;
		posBuild
			.add ("source", 1)
			.add ("target", 1);

		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);

		const auto source = vm ["source"].as<std::string> ();
		const auto target = vm ["target"].as<std::string> ();

		kylaRepairRepository (target.c_str (), source.c_str ());
	} else if (cmd == "query-filesets") {
		po::options_description build_desc ("query-filesets options");
		build_desc.add_options ()
			("source", po::value<std::string> ());

		po::positional_options_description posBuild;
		posBuild
			.add ("source", 1);

		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);

		const auto source = vm ["source"].as<std::string> ();

		int resultSize = 0;

		kylaQueryRepository (source.c_str (),
			kylaQueryRepositoryKey_AvailableFileSets, nullptr,
			&resultSize, nullptr);

		std::vector<kylaFileSetInfo> filesetInfos (resultSize / sizeof (kylaFileSetInfo));

		kylaQueryRepository (source.c_str (),
			kylaQueryRepositoryKey_AvailableFileSets, nullptr,
			&resultSize, filesetInfos.data ());

		for (const auto& info : filesetInfos) {
			std::cout << ToString (kyla::Uuid{ info.id }) << " " << info.fileCount << " " << info.fileSize << std::endl;
		}
	}
}
