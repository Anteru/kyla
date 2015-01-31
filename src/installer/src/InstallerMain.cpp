#include "Kyla.h"

#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>

////////////////////////////////////////////////////////////////////////////////
int main (int argc, char* argv [])
{
	namespace po = boost::program_options;
	po::options_description generic ("Generic options");
	generic.add_options ()
		("help,h", "Show help message");

	po::options_description desc ("Configuration");
	desc.add_options ()
		("log-level", po::value<int> ()->default_value (KylaLogLevelInfo))
		("log-file", po::value<std::string> ()->default_value ("install.log"))
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

	KylaInstaller* installer;
	kylaOpenInstallationPackage (inputFilePath.c_str (),
		&installer);

	KylaFeatures* features;
	kylaGetFeatures (installer, &features);

	int count;
	KylaFeature** firstFeature;

	kylaEnumerateFeatures(features, &count, &firstFeature);

	std::vector<KylaFeature*> selectedFeatures;
	for (int i = 0; i < count; ++i) {
		selectedFeatures.push_back (firstFeature [i]);
	}

	kylaSelectFeatures (installer, selectedFeatures.size (), selectedFeatures.data ());
	kylaDeleteFeatures (features);

	KylaProperty* targetDirectoryProperty = kylaCreateStringProperty (installationDirectory.c_str ());
	kylaSetProperty (installer, "TargetDirectory", targetDirectoryProperty);
	kylaDeleteProperty (targetDirectoryProperty);

	KylaProperty* stagingDirectoryProperty = kylaCreateStringProperty (stagingDirectory.c_str ());
	kylaSetProperty (installer, "StagingDirectory", stagingDirectoryProperty);
	kylaDeleteProperty (stagingDirectoryProperty);

	KylaProperty* sourcePackageDirectoryProperty = kylaCreateStringProperty (packageDirectory.c_str ());
	kylaSetProperty (installer, "SourcePackageDirectory", sourcePackageDirectoryProperty);
	kylaDeleteProperty (sourcePackageDirectoryProperty);

	kylaLog (installer, vm ["log-file"].as<std::string> ().c_str (),
			vm ["log-level"].as<int> ());

	kylaInstall (installer, nullptr);
	kylaCloseInstallationPackage (installer);
}
