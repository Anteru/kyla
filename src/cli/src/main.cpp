/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include <boost/program_options.hpp>

#include "Kyla.h"
#include "KylaBuild.h"

#include <iostream>
#include <iomanip>
#include "Uuid.h"

#include <chrono>
#include <iomanip>

///////////////////////////////////////////////////////////////////////////////
const char* kylaGetErrorString (const int r)
{
	switch (r) {
	case kylaResult_Ok: return "Ok";
	case kylaResult_Error: return "Error";
	case kylaResult_ErrorInvalidArgument: return "Invalid argument";
	case kylaResult_ErrorUnsupportedApiVersion: return "Unsupported Api version";
	default:
		return "Unknown error";
	}
}

#define KYLA_CHECKED_CALL(c) do {auto r = c; if (r != kylaResult_Ok) { throw std::runtime_error (kylaGetErrorString (r)); }} while (0)

namespace po = boost::program_options;

///////////////////////////////////////////////////////////////////////////////
void StdoutLog (const char* source, const kylaLogSeverity severity,
	const char* message, const int64_t timestamp, void*)
{
	std::chrono::nanoseconds timeSinceStart{ timestamp };
	const auto hours = std::chrono::duration_cast<std::chrono::hours> (timeSinceStart);
	timeSinceStart -= hours;
	const auto minutes = std::chrono::duration_cast<std::chrono::minutes> (timeSinceStart);
	timeSinceStart -= minutes;
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds> (timeSinceStart);
	timeSinceStart -= seconds;
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds> (timeSinceStart);
	
	std::cout << std::setw (2) << std::setfill ('0') << hours.count ()
		<< ':' << std::setw (2) << std::setfill ('0') << minutes.count ()
		<< ':' << std::setw (2) << std::setfill ('0') << seconds.count ()
		<< '.' << std::setw (3) << std::setfill ('0') << milliseconds.count ()
		<< " | ";

	switch (severity) {
	case kylaLogSeverity_Debug: std::cout	<< "Debug:   "; break;
	case kylaLogSeverity_Info: std::cout	<< "Info:    "; break;
	case kylaLogSeverity_Warning: std::cout << "Warning: "; break;
	case kylaLogSeverity_Error: std::cout	<< "Error:   "; break;
	}

	std::cout << source << ":" << message << "\n";
}

///////////////////////////////////////////////////////////////////////////////
void StdoutProgress (const KylaProgress* progress, void* context)
{
	static const char* padding = 
		"                                        ";
	//   0123456789012345678901234567890123456879

	std::cout << std::fixed << std::setprecision (2) << progress->totalProgress * 100
		<< " % : " << progress->action 
		<< (padding + std::min (::strlen (padding), ::strlen (progress->action))) << "\r";

	if (progress->totalProgress == 1.0) {
		std::cout << "\n";
	}
}

///////////////////////////////////////////////////////////////////////////////
int Build (const std::vector<std::string>& options,
	po::variables_map& vm)
{
	po::options_description build_desc ("build options");
	build_desc.add_options ()
		("statistics", po::value<bool> ()->default_value (false))
		("source-directory", po::value<std::string> ()->default_value ("."),
			"Source directory")
		("input", po::value<std::string> ())
		("target-directory", po::value<std::string> ());

	po::positional_options_description posBuild;
	posBuild
		.add ("input", 1)
		.add ("target-directory", 1);

	try {
		po::store (po::command_line_parser (options).options (build_desc)
			.positional (posBuild).run (), vm);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}

	KylaBuildStatistics statistics = {};

	KylaBuildSettings buildSettings = {};
	buildSettings.descriptorFile = vm ["input"].as<std::string> ().c_str ();
	buildSettings.sourceDirectory = vm ["source-directory"].as<std::string> ().c_str ();
	buildSettings.targetDirectory = vm ["target-directory"].as<std::string> ().c_str ();

	if (vm ["statistics"].as<bool> ()) {
		buildSettings.buildStatistics = &statistics;
	}

	const auto result = kylaBuildRepository (&buildSettings);

	if (vm ["statistics"].as<bool> ()) {
		std::cout << "Uncompressed:      " << statistics.uncompressedContentSize << std::endl;
		std::cout << "Compressed:        " << statistics.compressedContentSize << std::endl;
		std::cout << "Compression ratio: " << statistics.compressionRatio << std::endl;
		std::cout << "Compression time:  " << statistics.compressionTimeSeconds << " (sec)" << std::endl;
		std::cout << "Encryption time:   " << statistics.encryptionTimeSeconds << " (sec)" << std::endl;
		std::cout << "Hash time:         " << statistics.hashTimeSeconds << " (sec)" << std::endl;
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
int Validate (const std::vector<std::string>& options,
	po::variables_map& vm)
{
	po::options_description build_desc ("validation options");
	build_desc.add_options ()
		("verbose,v", po::bool_switch ()->default_value (false),
			"verbose output")
		("summary,s", po::value<bool> ()->default_value (true),
			"show summary")
		("key", po::value<std::string> ())
		("source", po::value<std::string> ())
		("target", po::value<std::string> ());

	po::positional_options_description posBuild;
	posBuild
		.add ("source", 1)
		.add ("target", 1);

	try {
		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}

	int errors = 0;
	int ok = 0;

	struct Context
	{
		int* errors;
		int* ok;
		bool verbose;
	};

	Context context = { &errors, &ok, vm ["verbose"].as<bool> () };

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

	KylaInstaller* installer = nullptr;
	KYLA_CHECKED_CALL (kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer));

	assert (installer);

	if (vm ["log"].as<bool> ()) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	installer->SetValidationCallback (installer, validationCallback, &context);

	KylaTargetRepository sourceRepository, targetRepository;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer,
		vm["source"].as<std::string> ().c_str (), 0, &sourceRepository));

	KYLA_CHECKED_CALL (installer->OpenTargetRepository (installer,
		vm ["target"].as<std::string> ().c_str (), 0, &targetRepository));

	if (vm.find ("key") != vm.end ()) {
		const auto key = vm ["key"].as<std::string> ();
	
		installer->SetVariable (
			installer, "Encryption.Key",
			key.size () + 1,
			key.c_str ()
		);
	}

	KYLA_CHECKED_CALL (installer->Execute (installer, kylaAction_Verify, 
		targetRepository, sourceRepository, nullptr));

	installer->CloseRepository (installer, sourceRepository);
	installer->CloseRepository (installer, targetRepository);
	KYLA_CHECKED_CALL (kylaDestroyInstaller (installer));

	if (vm ["summary"].as<bool> ()) {
		std::cout << "OK " << ok << " CORRUPTED/MISSING " << errors << std::endl;
	}

	return (errors == 0) ? 0 : 1;
}

///////////////////////////////////////////////////////////////////////////////
int Repair (const std::vector<std::string>& options,
	po::variables_map& vm)
{
	po::options_description build_desc ("repair options");
	build_desc.add_options ()
		("source", po::value<std::string> ())
		("target", po::value<std::string> ());

	po::positional_options_description posBuild;
	posBuild
		.add ("source", 1)
		.add ("target", 1);

	try {
		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}

	KylaInstaller* installer = nullptr;
	KYLA_CHECKED_CALL (kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer));

	assert (installer);

	if (vm ["log"].as<bool> ()) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	KylaTargetRepository source;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, 
		vm ["source"].as<std::string> ().c_str (), 0, &source));

	KylaTargetRepository target;
	KYLA_CHECKED_CALL (installer->OpenTargetRepository (installer, 
		vm ["target"].as<std::string> ().c_str (), 0, &target));

	const auto result = installer->Execute (installer, kylaAction_Repair, target, source,
		nullptr);

	installer->CloseRepository (installer, source);
	installer->CloseRepository (installer, target);
	kylaDestroyInstaller (installer);

	return result;
}

///////////////////////////////////////////////////////////////////////////////
int QueryRepository (const std::vector<std::string>& options,
	po::variables_map& vm)
{ 
	po::options_description build_desc ("query-repository options");
	build_desc.add_options ()
		("property", po::value<std::string> ())
		("source", po::value<std::string> ());

	po::positional_options_description posBuild;
	posBuild
		.add ("property", 1)
		.add ("source", 1);

	try {
		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}

	if (vm ["source"].empty ()) {
		std::cerr << "No repository specified" << std::endl;
		return 1;
	}

	KylaInstaller* installer = nullptr;
	kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer);

	assert (installer);

	if (vm ["log"].as<bool> ()) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	KylaTargetRepository source;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, 
		vm ["source"].as<std::string> ().c_str (),
		kylaRepositoryOption_ReadOnly, &source));

	const auto property = vm["property"].as<std::string> ();

	if (property == "features") {
		std::size_t resultSize = 0;
		KYLA_CHECKED_CALL (installer->GetRepositoryProperty (installer, source,
			kylaRepositoryProperty_AvailableFeatures, &resultSize, nullptr));

		std::vector<KylaUuid> features;
		features.resize (resultSize / sizeof (KylaUuid));
		KYLA_CHECKED_CALL (installer->GetRepositoryProperty (installer, source,
			kylaRepositoryProperty_AvailableFeatures, &resultSize, features.data ()));
	
		for (const auto& feature : features) {
			std::cout << ToString (kyla::Uuid{ feature.bytes }) << std::endl;
		}
	}

	installer->CloseRepository (installer, source);
	KYLA_CHECKED_CALL (kylaDestroyInstaller (installer));

	return kylaResult_Ok;
}

///////////////////////////////////////////////////////////////////////////////
int QueryFeature (const std::vector<std::string>& options,
	po::variables_map& vm)
{
	po::options_description build_desc ("query-feature options");
	build_desc.add_options ()
		("property", po::value<std::string> ())
		("feature-id", po::value<std::string> ())
		("source", po::value<std::string> ());

	po::positional_options_description posBuild;
	posBuild
		.add ("property", 1)
		.add ("feature-id", 1)
		.add ("source", 1);

	try {
		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}

	if (vm["source"].empty ()) {
		std::cerr << "No repository specified" << std::endl;
		return 1;
	}

	KylaInstaller* installer = nullptr;
	kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer);

	assert (installer);

	if (vm["log"].as<bool> ()) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	KylaSourceRepository source;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, vm["source"].as<std::string> ().c_str (),
		kylaRepositoryOption_ReadOnly, &source));

	KylaUuid featureId;
	{
		const auto tempId = kyla::Uuid::Parse (vm["feature-id"].as<std::string> ());
		::memcpy (featureId.bytes, tempId.GetData (), sizeof (featureId.bytes));
	}
	const auto property = vm["property"].as<std::string> ();
	
	if (property == "subfeatures") {
		size_t resultSize = 0;
		installer->GetFeatureProperty (installer, source,
			featureId, kylaFeatureProperty_SubfeatureIds, &resultSize, nullptr);

		std::vector<KylaUuid> subfeatures;
		subfeatures.resize (resultSize / sizeof (KylaUuid));

		installer->GetFeatureProperty (installer, source,
			featureId, kylaFeatureProperty_SubfeatureIds, 
			&resultSize, subfeatures.data ());

		for (const auto& subfeature : subfeatures) {
			std::cout << ToString (kyla::Uuid (subfeature.bytes));
		}
	} else if (property == "size") {
		int64_t result = 0;
		size_t resultSize = sizeof (result);

		installer->GetFeatureProperty (installer, source,
			featureId, kylaFeatureProperty_Size, &resultSize, &result);

		std::cout << result << std::endl;
	}

	installer->CloseRepository (installer, source);
	KYLA_CHECKED_CALL (kylaDestroyInstaller (installer));

	return kylaResult_Ok;
}

///////////////////////////////////////////////////////////////////////////////
int ConfigureOrInstall (const std::string& cmd,
	const std::vector<std::string>& options,
	po::variables_map& vm)
{
	po::options_description build_desc ("install options");
	build_desc.add_options ()
		("key", po::value<std::string> ())
		("source", po::value<std::string> ())
		("target", po::value<std::string> ())
		("features", po::value<std::vector<std::string>> ()->composing ());

	po::positional_options_description posBuild;
	posBuild
		.add ("source", 1)
		.add ("target", 1)
		.add ("features", -1);

	try {
		po::store (po::command_line_parser (options).options (build_desc).positional (posBuild).run (), vm);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}

	KylaInstaller* installer = nullptr;
	KYLA_CHECKED_CALL (kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer));

	assert (installer);

	if (vm ["log"].as<bool> ()) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	if (vm ["progress"].as<bool> ()) {
		installer->SetProgressCallback (installer, StdoutProgress, nullptr);
	}

	KylaSourceRepository sourceRepository;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, 
		vm ["source"].as<std::string> ().c_str (), 0, &sourceRepository));

	if (vm.find ("key") != vm.end ()) {
		const auto key = vm["key"].as<std::string> ();

		installer->SetVariable (
			installer, "Encryption.Key",
			key.size () + 1,
			key.c_str ()
		);
	}

	KylaTargetRepository targetRepository;
	KYLA_CHECKED_CALL (installer->OpenTargetRepository (installer, 
		vm ["target"].as<std::string> ().c_str (), 
		cmd == "install" ? kylaRepositoryOption_Create : 0, &targetRepository));

	const auto features = vm ["features"].as<std::vector<std::string>> ();

	std::vector<const uint8_t*> featurePointers;
	std::vector<kyla::Uuid> featureIds;
	for (const auto feature : features) {
		featureIds.push_back (kyla::Uuid::Parse (feature));
	}

	for (const auto& featureId : featureIds) {
		featurePointers.push_back (featureId.GetData ());
	}

	KylaDesiredState desiredState = {};
	desiredState.featureCount = static_cast<int> (featureIds.size ());
	desiredState.featureIds = featurePointers.data ();

	int result = -1;

	if (cmd == "install") {
		result = installer->Execute (installer, kylaAction_Install,
			targetRepository, sourceRepository, &desiredState);
	} else {
		result = installer->Execute (installer, kylaAction_Configure,
			targetRepository, sourceRepository, &desiredState);
	}

	installer->CloseRepository (installer, sourceRepository);
	installer->CloseRepository (installer, targetRepository);
	KYLA_CHECKED_CALL (kylaDestroyInstaller (installer));

	return result;
}

///////////////////////////////////////////////////////////////////////////////
int main (int argc, char* argv [])
{
	po::options_description global ("Global options");
	global.add_options ()
		("log,l", po::bool_switch ()->default_value (false), "Show log output")
		("progress,p", po::bool_switch ()->default_value (false), "Show progress")
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

	try {
		po::store (parsed, vm);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}

	if (vm.find ("command") == vm.end ()) {
		global.print (std::cout);
		return 0;
	}

	const auto cmd = vm ["command"].as<std::string> ();

	auto options = po::collect_unrecognized (parsed.options, po::include_positional);
	// Remove the command name
	options.erase (options.begin ());

	try {
		if (cmd == "build") {
			return Build (options, vm);
		} else if (cmd == "validate") {
			return Validate (options, vm);
		} else if (cmd == "repair") {
			return Repair (options, vm);
		} else if (cmd == "query-repository") {
			return QueryRepository (options, vm);
		} else if (cmd == "query-feature") {
			return QueryFeature (options, vm);
		} else if (cmd == "install" || cmd == "configure") {
			return ConfigureOrInstall (cmd, options, vm);
		} else {
			std::cerr << "No command was specified" << std::endl;
			return 1;
		}
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}
}
