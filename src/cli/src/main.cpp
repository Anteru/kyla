/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include <CLI/CLI.hpp>

#include "Kyla.h"
#include "KylaBuild.h"

#include <iostream>
#include <iomanip>
#include "Uuid.h"

#include <chrono>
#include <iomanip>

#include <cassert>

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

///////////////////////////////////////////////////////////////////////////////
void StdoutLog (const KylaLog* log, void*)
{
	std::chrono::nanoseconds timeSinceStart{ log->timestamp };
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

	switch (log->severity) {
	case kylaLogSeverity_Debug: std::cout	<< "Debug:   "; break;
	case kylaLogSeverity_Info: std::cout	<< "Info:    "; break;
	case kylaLogSeverity_Warning: std::cout << "Warning: "; break;
	case kylaLogSeverity_Error: std::cout	<< "Error:   "; break;
	}

	std::cout << log->source << ":" << log->message << "\n";
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
int Build (const bool showStatistics,
	const std::string& sourceDirectory,
	const std::string& input,
	const std::string& targetDirectory)
{
	KylaBuildStatistics statistics = {};

	KylaBuildSettings buildSettings = {};
	buildSettings.descriptorFile = input.c_str ();
	buildSettings.sourceDirectory = sourceDirectory.c_str ();
	buildSettings.targetDirectory = targetDirectory.c_str ();

	if (showStatistics) {
		buildSettings.buildStatistics = &statistics;
	}

	const auto result = kylaBuildRepository (&buildSettings);

	if (showStatistics) {
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
int Validate (const bool verbose,
	const bool showSummary,
	const bool log,
	const std::string& key,
	const std::string& source,
	const std::string& target)
{
	int errors = 0;
	int ok = 0;

	struct Context
	{
		int* errors;
		int* ok;
		bool verbose;
	};

	Context context = { &errors, &ok, verbose };

	auto validationCallback = [](const KylaValidation* validation,
		void* pContext) -> void {
		auto context = static_cast<Context*> (pContext);

		switch (validation->itemType) {
		case kylaValidationItemType_File:
			const auto& info = validation->infoFile;
			switch (validation->result) {
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
		}
		
	};

	KylaInstaller* installer = nullptr;
	KYLA_CHECKED_CALL (kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer));

	assert (installer);

	if (log) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	installer->SetValidationCallback (installer, validationCallback, &context);

	KylaTargetRepository sourceRepository, targetRepository;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer,
		source.c_str (), 0, &sourceRepository));

	KYLA_CHECKED_CALL (installer->OpenTargetRepository (installer,
		target.c_str (), 0, &targetRepository));

	if (! key.empty() ) {
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

	if (showSummary) {
		std::cout << "OK " << ok << " CORRUPTED/MISSING " << errors << std::endl;
	}

	return (errors == 0) ? 0 : 1;
}

///////////////////////////////////////////////////////////////////////////////
int Repair (const bool showLog,
	const std::string& sourcePath,
	const std::string& targetPath)
{
	KylaInstaller* installer = nullptr;
	KYLA_CHECKED_CALL (kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer));

	assert (installer);

	if (showLog) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	KylaTargetRepository source;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, 
		sourcePath.c_str (), 0, &source));

	KylaTargetRepository target;
	KYLA_CHECKED_CALL (installer->OpenTargetRepository (installer, 
		targetPath.c_str (), 0, &target));

	const auto result = installer->Execute (installer, kylaAction_Repair, target, source,
		nullptr);

	installer->CloseRepository (installer, source);
	installer->CloseRepository (installer, target);
	kylaDestroyInstaller (installer);

	return result;
}

///////////////////////////////////////////////////////////////////////////////
int QueryRepository (
	const bool showLog,
	const std::string& property,
	const std::string& sourcePath)
{ 
	KylaInstaller* installer = nullptr;
	kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer);

	assert (installer);

	if (showLog) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	KylaTargetRepository source;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, 
		sourcePath.c_str (),
		kylaRepositoryOption_ReadOnly, &source));

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
int QueryFeature (const bool showLog,
	const std::string& property,
	const std::string& featureIdString,
	const std::string& sourcePath)
{
	KylaInstaller* installer = nullptr;
	kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer);

	assert (installer);

	if (showLog) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	KylaSourceRepository source;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, 
		sourcePath.c_str (),
		kylaRepositoryOption_ReadOnly, &source));

	KylaUuid featureId;
	{
		const auto tempId = kyla::Uuid::Parse (featureIdString);
		::memcpy (featureId.bytes, tempId.GetData (), sizeof (featureId.bytes));
	}
	
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
int ConfigureOrInstall (
	const bool showLog,
	const bool showProgress,
	const std::string& key,
	const std::string& sourcePath,
	const std::string& targetPath,
	const std::string& cmd,
	const std::vector<std::string>& features)
{
	KylaInstaller* installer = nullptr;
	KYLA_CHECKED_CALL (kylaCreateInstaller (KYLA_API_VERSION_3_0, &installer));

	assert (installer);

	if (showLog) {
		installer->SetLogCallback (installer, StdoutLog, nullptr);
	}

	if (showProgress) {
		installer->SetProgressCallback (installer, StdoutProgress, nullptr);
	}

	KylaSourceRepository sourceRepository;
	KYLA_CHECKED_CALL (installer->OpenSourceRepository (installer, 
		sourcePath.c_str (), 0, &sourceRepository));

	if (! key.empty()) {
		installer->SetVariable (
			installer, "Encryption.Key",
			key.size () + 1,
			key.c_str ()
		);
	}

	KylaTargetRepository targetRepository;
	KYLA_CHECKED_CALL (installer->OpenTargetRepository (installer, 
		targetPath.c_str (), 
		cmd == "install" ? kylaRepositoryOption_Create : 0, &targetRepository));

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
	CLI::App app;

	bool log = false;
	app.add_flag ("-l,--log", log, "Show log output");

	bool progress = false;
	app.add_flag ("-p,--progress", progress, "Show progress output");

	bool verbose = false;
	app.add_flag ("-v,--verbose", verbose, "Show verbose output");

	auto buildCmd = app.add_subcommand ("build");
	bool showStatistics = false;
	buildCmd->add_flag ("-s,--statistics", showStatistics, "Show statistics");
	std::string sourceDirectory, input, targetDirectory;
	buildCmd->add_option ("--source-directory", sourceDirectory, "Source directory");
	buildCmd->add_option ("INPUT", input, "Input file")->check (CLI::ExistingFile);
	buildCmd->add_option ("TARGET_DIRECTORY", targetDirectory, "Target directory");
	buildCmd->callback ([&] () -> void {
		exit (Build (showStatistics, sourceDirectory, input, targetDirectory));
	});

	std::string key;
	std::string sourcePath, targetPath;

	auto validateCmd = app.add_subcommand ("validate");
	bool showSummary = false;
	validateCmd->add_flag ("-s,--summary", showSummary, "Show summary");
	validateCmd->add_option ("-k,--key", key, "Encryption key");
	validateCmd->add_option ("SOURCE_REPOSITORY", sourcePath, "Source repository path");
	validateCmd->add_option ("TARGET_REPOSITORY", targetPath, "Target repository path");

	validateCmd->callback ([&] () -> void {
		exit (Validate (verbose, showSummary, log, key, sourcePath, targetPath));
	});

	auto repairCmd = app.add_subcommand ("repair");
	repairCmd->add_option ("SOURCE_REPOSITORY", sourcePath, "Source repository path");
	repairCmd->add_option ("TARGET_REPOSITORY", targetPath, "Target repository path");

	repairCmd->callback ([&]() -> void {
		exit (Repair (log, sourcePath, targetPath));
	});

	auto queryRepositoryCmd = app.add_subcommand ("query-repository");
	std::string property;

	queryRepositoryCmd->add_option ("PROPERTY", property, "The property to query");
	queryRepositoryCmd->add_option ("SOURCE_REPOSITORY", sourcePath, "Source repository path");

	queryRepositoryCmd->callback ([&]() -> void {
		exit (QueryRepository (log, property, sourcePath));
		});

	auto queryFeatureCmd = app.add_subcommand ("query-feature");
	std::string featureId;

	queryFeatureCmd->add_option ("PROPERTY", property, "The property to query");
	queryFeatureCmd->add_option ("FEATURE_ID", featureId, "The feature ID to query");
	queryFeatureCmd->add_option ("SOURCE_REPOSITORY", sourcePath, "Source repository path");

	queryFeatureCmd->callback ([&]() -> void {
		exit (QueryFeature (log, property, featureId, sourcePath));
	});

	auto installCmd = app.add_subcommand ("install");
	std::vector<std::string> features;

	installCmd->add_option ("-k,--key", key, "Encryption key");
	installCmd->add_option ("SOURCE_REPOSITORY", sourcePath, "Source repository path");
	installCmd->add_option ("TARGET_REPOSITORY", targetPath, "Target repository path");
	installCmd->add_option ("FEATURES", features, "The features to install");
	
	installCmd->callback ([&]() -> void {
		exit (ConfigureOrInstall (log, progress, key, sourcePath, targetPath, "install", features));
		});

	auto configureCmd = app.add_subcommand ("configure");

	configureCmd->add_option ("-k,--key", key, "Encryption key");
	configureCmd->add_option ("SOURCE_REPOSITORY", sourcePath, "Source repository path");
	configureCmd->add_option ("TARGET_REPOSITORY", targetPath, "Target repository path");
	configureCmd->add_option ("FEATURES", features, "The features to configure");

	configureCmd->callback ([&]() -> void {
		exit (ConfigureOrInstall (log, progress, key, sourcePath, targetPath, "configure", features));
		});

	try {
		CLI11_PARSE (app, argc, argv);
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}
}
