#include <boost/program_options.hpp>

#include "Kyla.h"

#include <iostream>
#include "Uuid.h"

extern int kylaBuildRepository (const char* repositoryDescription,
	const char* sourceDirectory, const char* targetDirectory);

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
		
		kylaBuildRepository (vm ["input"].as<std::string> ().c_str (),
			vm ["source-directory"].as<std::string> ().c_str (),
			vm ["output-directory"].as<std::string> ().c_str ());
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

		auto validationCallback = [](kylaValidationResult validationResult,
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

		KylaInstaller* installer;
		kylaCreateInstaller (KYLA_API_VERSION_1_0, &installer);

		KylaTargetRepository repository;
		installer->OpenTargetRepository (installer, vm ["input"].as<std::string> ().c_str (),
			0, &repository);

		installer->SetValidationCallback (installer, validationCallback, &context);
		installer->Execute (installer, kylaAction_Verify, repository, nullptr, nullptr);

		installer->CloseRepository (installer, repository);
		kylaDestroyInstaller (installer);

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

		KylaInstaller* installer;
		kylaCreateInstaller (KYLA_API_VERSION_1_0, &installer);

		KylaTargetRepository source;
		installer->OpenSourceRepository (installer, vm ["source"].as<std::string> ().c_str (),
			0, &source);

		KylaTargetRepository target;
		installer->OpenTargetRepository (installer, vm ["target"].as<std::string> ().c_str (),
			0, &target);

		installer->Execute (installer, kylaAction_Repair, target, source,
			nullptr);

		installer->CloseRepository (installer, source);
		installer->CloseRepository (installer, target);
		kylaDestroyInstaller (installer);
	} else if (cmd == "query-filesets") {
		po::options_description build_desc ("query-filesets options");
		build_desc.add_options ()
			("source", po::value<std::string> ())
			("name,n", po::bool_switch ()->default_value (false),
				"query the fileset name as well");

		po::positional_options_description posBuild;
		posBuild
			.add ("source", 1);

		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);

		KylaInstaller* installer;
		kylaCreateInstaller (KYLA_API_VERSION_1_0, &installer);

		KylaTargetRepository source;
		installer->OpenSourceRepository (installer, vm ["source"].as<std::string> ().c_str (),
			0, &source);

		int filesetCount = 0;

		installer->QueryFilesets (installer, source,
			&filesetCount, nullptr);

		std::vector<KylaFilesetInfo> filesetInfos{ static_cast<size_t> (filesetCount) };

		installer->QueryFilesets (installer, source,
			&filesetCount, filesetInfos.data ());

		const auto queryName = vm ["name"].as<bool> ();

		for (const auto& info : filesetInfos) {
			if (queryName) {
				int nameSize = 0;

				installer->QueryFilesetName (installer, source,
					info.id, &nameSize, nullptr);
				std::vector<char> name;
				name.resize (nameSize);

				installer->QueryFilesetName (installer, source,
					info.id, &nameSize, name.data ());

				std::cout << ToString (kyla::Uuid{ info.id }) << " " << name.data ();
			} else {
				std::cout << ToString (kyla::Uuid{ info.id });
			}

			std::cout << " " << info.fileCount << " " << info.fileSize << std::endl;
		}

		installer->CloseRepository (installer, source);
		kylaDestroyInstaller (installer);
	} else if (cmd == "install" || cmd == "configure") {
		po::options_description build_desc ("install options");
		build_desc.add_options ()
			("source", po::value<std::string> ())
			("target", po::value<std::string> ())
			("file-sets", po::value<std::vector<std::string>> ()->composing ());

		po::positional_options_description posBuild;
		posBuild
			.add ("source", 1)
			.add ("target", 1)
			.add ("file-sets", -1);

		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);

		KylaInstaller* installer;
		kylaCreateInstaller (KYLA_API_VERSION_1_0, &installer);

		KylaSourceRepository source;
		installer->OpenSourceRepository (installer, vm ["source"].as<std::string> ().c_str (),
			0, &source);

		KylaTargetRepository target;
		installer->OpenTargetRepository (installer, vm ["target"].as<std::string> ().c_str (),
			0, &target);

		const auto filesets = vm ["file-sets"].as<std::vector<std::string>> ();

		std::vector<const uint8_t*> filesetPointers;
		std::vector<kyla::Uuid> filesetIds;
		for (const auto fileset : filesets) {
			filesetIds.push_back (kyla::Uuid::Parse (fileset));
		}

		for (const auto& filesetId : filesetIds) {
			filesetPointers.push_back (filesetId.GetData ());
		}

		KylaDesiredState desiredState = {};
		desiredState.filesetCount = static_cast<int> (filesetIds.size ());
		desiredState.filesetIds = filesetPointers.data ();

		if (cmd == "install") {
			installer->Execute (installer, kylaAction_Install,
				target, source, &desiredState);
		} else {
			installer->Execute (installer, kylaAction_Configure,
				target, source, &desiredState);
		}

		installer->CloseRepository (installer, source);
		installer->CloseRepository (installer, target);
		kylaDestroyInstaller (installer);
	}
}
