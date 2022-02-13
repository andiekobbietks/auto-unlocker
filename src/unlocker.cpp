/************************************************************************************************
	Unlocker - Patcher + Tools Downloader
	Created by Paolo Infante

	Based on "Unlocker" by DrDonk for a native solution to python errors and
	virus warnings.

	I coded from scratch all the patch routines (I tried to be consistent with C++ library usage
	though you'll probably find a few memcpy here and there...) and tools download (using
	libcurl for download and libarchive to uncompress the archives)

	Should be cross-platform. Tested on Windows, I'm planning to test it on linux too
	I tried my best to use STL whenever possible. I had to use external libraries:
	libcurl, libarchive and zlib that I'm sure can be compiled on linux, though.

	To ensure cross-platform fs access I used std::filesystem from c++17, so a compiler that
	supports it is required.

	If the case arises that I'm not able to use STL I'll try to find a multiplatform solution
	or code different solutions for both win and linux (as I did for Registry access)

*************************************************************************************************/

#include "unlocker.h"

// Main function

void install()
{
	// Default output path is ./tools/
	fs::path toolsdirectory = fs::path(".") / TOOLS_DOWNLOAD_FOLDER;
	// Default backup path is ./backup/
	fs::path backup = fs::path(".") / BACKUP_FOLDER;

	logi("Killing services and backing up files...");
	preparePatch(backup);

	logi("Patching files...");
	doPatch();

	logi("Downloading tools into \"" + toolsdirectory.string() + "\" directory...");

	bool alreadyHasTools = false;

	if (fs::exists(fs::path(".") / TOOLS_DOWNLOAD_FOLDER / FUSION_ZIP_TOOLS_NAME) && fs::exists(fs::path(".") / TOOLS_DOWNLOAD_FOLDER / FUSION_ZIP_PRE15_TOOLS_NAME))
	{
		std::cout << "Tools have been found in current folder. Do you want to use them? Answering n will download them again" << std::endl
			<< "Please check that the existing tools are working and are the most recent ones." << std::endl
			<< "(y/n) ";

		std::string c;
		std::cin >> c;

		if (c == "y" || c == "Y")
		{
			alreadyHasTools = true;
		}
	}

	if(!alreadyHasTools)
	{
		downloadTools(toolsdirectory);
	}

	logi("Copying tools into program directory...");
	copyTools(toolsdirectory);

	restartServices();

	logi("Patch complete.");
}

void uninstall()
{
#ifdef _WIN32
	VMWareInfoRetriever vmInfo;

	// Stop services
	stopServices();

	// Default output path is ./tools/
	fs::path toolsdirectory = fs::path(".") / TOOLS_DOWNLOAD_FOLDER;
	// Default backup path is ./backup/
	fs::path backup = fs::path(".") / BACKUP_FOLDER;

	fs::path vmwareInstallDir = vmInfo.getInstallPath();
	fs::path vmwareInstallDir64 = vmInfo.getInstallPath64();

	logi("Restoring files...");
	// Copy contents of backup/
	if (fs::exists(backup))
	{
		for (const auto& file : fs::directory_iterator(backup))
		{
			if (fs::is_regular_file(file))
			{
				try
				{
					if (fs::copy_file(file.path(), vmwareInstallDir / file.path().filename(), fs::copy_options::overwrite_existing))
					{
						logi("File \"" + file.path().string() + "\" restored successfully");
					}
					else
					{
						logerr("Error while restoring \"" + file.path().string() + "\".");
					}
				}
				catch (const fs::filesystem_error& ex)
				{
					logerr(ex.what());
				}
			}
		}
		// Copy contents of backup/x64/
		for (const auto& file : fs::directory_iterator(backup / "x64"))
		{
			if (fs::is_regular_file(file))
			{
				try
				{
					if (fs::copy_file(file.path(), vmwareInstallDir64 / file.path().filename(), fs::copy_options::overwrite_existing))
					{
						logi("File \"" + file.path().string() + "\" restored successfully");
					}
					else
					{
						logerr("Error while restoring \"" + file.path().string() + "\".");
					}
				}
				catch (const fs::filesystem_error& ex)
				{
					logerr(ex.what());
				}
			}
		}
	}
	else {
		logerr("Couldn't find backup files...");
		return;
	}
	// Remove darwin*.* from InstallDir
	for (const auto& file : fs::directory_iterator(vmwareInstallDir))
	{
		if (fs::is_regular_file(file))
		{
			size_t is_drw = file.path().filename().string().find("darwin");
			if (is_drw != std::string::npos && is_drw == 0)
			{
				fs::remove(file);
			}
		}
	}

	fs::remove_all(backup);
	fs::remove_all(toolsdirectory);

	// Restart services
	restartServices();

	logi("Uninstall complete.");
#elif defined (__linux__)
	// Default output path is ./tools/
	fs::path toolsdirectory = fs::path(".") / TOOLS_DOWNLOAD_FOLDER;
	// Default backup path is ./backup/
	fs::path backup = fs::path(".") / BACKUP_FOLDER;

	fs::path vmwareDir = VM_LNX_PATH;

	logi("Restoring files...");

	// Copy contents of backup/
	std::vector<std::string> lnxBins = VM_LNX_BINS;
	for (const auto& file : lnxBins)
	{
		try
		{
			if (fs::copy_file(backup / file, vmwareDir / file, fs::copy_options::overwrite_existing))
			{
				logi("File \"" + (backup / file).string() + "\" restored successfully");
			}
			else
			{
				logerr("Error while restoring \"" + (backup / file).string() + "\".");
			}
		}
		catch (const fs::filesystem_error& ex)
		{
			logerr(ex.what());
		}
	}
	std::vector<std::string> vmLibCandidates = VM_LNX_LIB_CANDIDATES;
	for (const auto& lib : vmLibCandidates)
	{
		if (fs::exists(fs::path(lib).parent_path()))
		{
			try
			{
				if (fs::copy_file(backup / fs::path(lib).filename(), fs::path(lib), fs::copy_options::overwrite_existing))
				{
					logi("File \"" + (backup / fs::path(lib).filename()).string() + "\" restored successfully");
				}
				else
				{
					logerr("Error while restoring \"" + (backup / fs::path(lib).filename()).string() + "\".");
				}
			}
			catch (const fs::filesystem_error& ex)
			{
				logerr(ex.what());
			}
			break;
		}
	}

	// Remove darwin*.* from InstallDir
	for (const auto& file : fs::directory_iterator(VM_LNX_ISO_DESTPATH))
	{
		if (fs::is_regular_file(file))
		{
			size_t is_drw = file.path().filename().string().find("darwin");
			if (is_drw != std::string::npos && is_drw == 0)
			{
				fs::remove(file);
			}
		}
	}

	fs::remove_all(backup);
	fs::remove_all(toolsdirectory);

	logi("Uninstall complete.");
#endif
}

void showhelp()
{
	std::cout << "auto-unlocker" << std::endl << std::endl
		<< "Run the program with one of these options:" << std::endl
		<< "	--install (default): install the patch" << std::endl
		<< "	--uninstall: remove the patch" << std::endl
		<< "	--download-tools: only download the tools" << std::endl
		<< "	--help: show this help message" << std::endl;
}

// Other methods

// Copy tools to VMWare directory
void copyTools(fs::path toolspath)
{
	fs::path toolsfrom = toolspath;
#ifdef _WIN32
	VMWareInfoRetriever vmInfo;
	fs::path copyto = vmInfo.getInstallPath();
#elif defined(__linux__)
	fs::path copyto = VM_LNX_ISO_DESTPATH;
#endif

	try
	{
		if (fs::copy_file(toolsfrom / FUSION_ZIP_TOOLS_NAME, copyto / FUSION_ZIP_TOOLS_NAME, fs::copy_options::overwrite_existing))
		{
			logi("File \"" + (toolsfrom / FUSION_ZIP_TOOLS_NAME).string() + "\" copy done.");
		}
		else
		{
			logerr("File \"" + (toolsfrom / FUSION_ZIP_TOOLS_NAME).string() + "\" could not be copied.");
		}
	}
	catch (const std::exception& e)
	{
		logerr(e.what());
	}

	try
	{
		if (fs::copy_file(toolsfrom / FUSION_ZIP_PRE15_TOOLS_NAME, copyto / FUSION_ZIP_PRE15_TOOLS_NAME, fs::copy_options::overwrite_existing))
		{
			logi("File \"" + (toolsfrom / FUSION_ZIP_PRE15_TOOLS_NAME).string() + "\" copy done.");
		}
		else
		{
			logerr("File \"" + (toolsfrom / FUSION_ZIP_PRE15_TOOLS_NAME).string() + "\" could not be copied.");
		}
	}
	catch (const std::exception& e)
	{
		logerr(e.what());
	}
}

// Actual patch code
void doPatch()
{
#ifdef _WIN32
	// Setup paths
	VMWareInfoRetriever vmInfo;

	std::string binList[] = VM_WIN_PATCH_FILES;

	fs::path vmwarebase_path = vmInfo.getInstallPath();
	fs::path vmx_path = vmInfo.getInstallPath64();
	fs::path vmx = vmx_path / binList[0];
	fs::path vmx_debug = vmx_path / binList[1];
	fs::path vmx_stats = vmx_path / binList[2];
	fs::path vmwarebase = vmwarebase_path / binList[3];

	if (!fs::exists(vmx))
	{
		throw std::runtime_error("Vmx file not found");
	}
	if (!fs::exists(vmx_debug))
	{
		throw std::runtime_error("Vmx-debug file not found");
	}
	if (!fs::exists(vmwarebase))
	{
		throw std::runtime_error("vmwarebase.dll file not found");
	}

	logi("File: " + vmx.filename().string());
	CHECKRES(Patcher::patchSMC(vmx, false));

	logi("File: " + vmx_debug.filename().string());
	CHECKRES(Patcher::patchSMC(vmx_debug, false));

	if (fs::exists(vmx_stats))
	{
		logi("File: " + vmx_stats.filename().string());
		CHECKRES(Patcher::patchSMC(vmx_stats, false));
	}

	logi("File: " + vmwarebase.filename().string());
	CHECKRES(Patcher::patchBase(vmwarebase));

#elif defined (__linux__)
	fs::path vmBinPath = VM_LNX_PATH;

	std::string binList[] = VM_LNX_BINS;

	fs::path vmx = vmBinPath / binList[0];
	fs::path vmx_debug = vmBinPath / binList[1];
	fs::path vmx_stats = vmBinPath / binList[2];

	// See if first candidate is good else use second one
	std::string libCandidates[] = VM_LNX_LIB_CANDIDATES;

	bool vmxso = true;

	fs::path vmlib = libCandidates[0];
	if (!fs::exists(vmlib)) {
		vmlib = libCandidates[1];
		vmxso = false;
	}

	if (!fs::exists(vmx))
	{
		throw std::runtime_error("Vmx file not found");
	}
	if (!fs::exists(vmx_debug))
	{
		throw std::runtime_error("Vmx-debug file not found");
	}
	if (!fs::exists(vmlib))
	{
		throw std::runtime_error("Vmlib file not found");
	}

	logi("File: " + vmx.filename().string());
	CHECKRES(Patcher::patchSMC(vmx, vmxso));

	logi("File: " + vmx_debug.filename().string());
	CHECKRES(Patcher::patchSMC(vmx_debug, vmxso));

	if (fs::exists(vmx_stats))
	{
		logi("File: " + vmx_stats.filename().string());
		CHECKRES(Patcher::patchSMC(vmx_stats, vmxso));
	}

	logi("File: " + vmlib.filename().string());
	CHECKRES(Patcher::patchBase(vmlib));
#else
	logerr("OS not supported");
	exit(1);
#endif
}

void stopServices()
{
#ifdef _WIN32
	// Stop services
	logi("Stopping services...");
	auto srvcList = std::list<std::string> VM_KILL_SERVICES;
	for (auto service : srvcList)
	{
		try
		{
			ServiceStopper::StopService_s(service);
			logd("Service \"" + service + "\" stopped successfully.");
		}
		catch (const ServiceStopper::ServiceStopException& ex)
		{
			// There is no need to inform the user that the service cannot be stopped if that service does not exist in the current version.
			//logerr("Couldn't stop service \"" + service + "\", " + std::string(ex.what()));
		}

	}

	auto procList = std::list<std::string> VM_KILL_PROCESSES;
	for (auto process : procList)
	{
		try {
			if (ServiceStopper::KillProcess(process))
			{
				logd("Process \"" + process + "\" killed successfully.");
			}
			else
			{
				logerr("Could not kill process \"" + process + "\".");
			}
		}
		catch (const ServiceStopper::ServiceStopException& ex)
		{
			logerr(ex.what());
		}
	}
#endif
}

void restartServices()
{
#ifdef _WIN32
	logi("Restarting services...");
	std::vector<std::string> servicesToStart = VM_KILL_SERVICES;
	for (auto it = servicesToStart.rbegin(); it != servicesToStart.rend(); it++)
	{
		try
		{
			ServiceStopper::StartService_s(*it);
			logd("Service \"" + *it + "\" started successfully.");
		}
		catch (const ServiceStopper::ServiceStopException& ex)
		{
			// There is no need to inform the user that the service cannot be started if that service does not exist in the current version.
			//logerr("Couldn't start service " + *it);
		}
	}
#endif
}

// Kill services / processes and backup files
void preparePatch(fs::path backupPath)
{
#ifdef _WIN32
	// Retrieve installation path from registry
	VMWareInfoRetriever vmInstall;

	stopServices();

	// Backup files
	auto filesToBackup = std::map<std::string, std::string> VM_WIN_BACKUP_FILES;

	for (auto element : filesToBackup)
	{
		auto filen = element.first;
		fs::path fPath = (vmInstall.getInstallPath() + filen);
		fs::path destpath = backupPath / element.second;
		if (!fs::exists(destpath))
			fs::create_directory(destpath);

		try
		{
			if (fs::copy_file(fPath, destpath / fPath.filename(), fs::copy_options::overwrite_existing))
			{
				logi("File \"" + fPath.string() + "\" backup done.");
			}
			else
			{
				logerr("File \"" + fPath.string() + "\" could not be copied.");
			}
		}
		catch (const std::exception& e)
		{
			logerr(e.what());
		}
	}
#elif defined (__linux__)
	// Backup files
	std::string filesToBackup[] = VM_LNX_BACKUP_FILES;
	fs::path destpath = backupPath;

	for (auto element : filesToBackup)
	{
		fs::path fPath = element;
		if (!fs::exists(destpath))
			fs::create_directory(destpath);

		try
		{
			if (fs::copy_file(fPath, destpath / fPath.filename(), fs::copy_options::overwrite_existing))
			{
				logi("File \"" + fPath.string() + "\" backup done.");
			}
			else
			{
				logerr("File \"" + fPath.string() + "\" could not be copied.");
			}
		}
		catch (const std::exception& e)
		{
			logerr(e.what());
		}
	}

	std::string libsAlternatives[] = VM_LNX_LIB_CANDIDATES;

	for (auto lib : libsAlternatives)
	{
		fs::path libpath = lib;
		if (fs::exists(libpath.parent_path()))
		{
			try
			{
				if (fs::copy_file(libpath, destpath / libpath.filename(), fs::copy_options::overwrite_existing))
				{
					logi("File \"" + libpath.string() + "\" backup done.");
				}
				else
				{
					logerr("File \"" + libpath.string() + "\" could not be copied.");
				}

				break;
			}
			catch (const std::exception& e)
			{
				logerr(e.what());
			}
		}
	}
#else
	// Either the compiler definitions are not working or the the tool is being compiled on an OS where it's not meant to be compiled
	logerr("OS not supported");
	exit(1);
#endif
}

// Download tools into "path"
bool downloadTools(fs::path path)
{
	Network network;

	fs::path temppath = fs::temp_directory_path(); // extract files in the temp folder first

	fs::create_directory(path); // create destination directory if it doesn't exist

	std::string url = FUSION_BASE_URL;

	std::string releasesList;

	releasesList = network.curlGet(url); // get the releases HTML page

	VersionParser versionParser(releasesList); // parse HTML page to version numbers

	if (versionParser.size() == 0)
	{
		logerr("No Fusion versions found in Download url location.");
		return false;
	}

	bool success = false;

	// For each version number retrieve the latest build and check if tools are available
	for (auto it = versionParser.cbegin(); it != versionParser.cend(); it++)
	{
		std::string version = it->getVersionText();

		ToolsDownloader downloader(network, FUSION_BASE_URL, version);

		if (downloader.download(path)) {
			success = true;
			break;
		}
	}

	if (success) {
		logi("Tools successfully downloaded!");
	}
	else {
		logerr("Couldn't find tools.");
	}

	return success;
}
