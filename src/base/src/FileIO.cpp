/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "FileIO.h"

#if KYLA_PLATFORM_LINUX
	#include <sys/mman.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
#elif KYLA_PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#undef CreateFile
	#undef min
	#undef max
#else
#error Unsupported platform
#endif

#include <algorithm>
#include <unordered_map>

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
File::File ()
{
}

////////////////////////////////////////////////////////////////////////////////
File::~File ()
{
}

FileStat Stat (const Path& path)
{
	return Stat (path.string ().c_str ());
}

FileStat Stat (const char* path)
{
	struct stat stats;
	::stat (path, &stats);

	FileStat result;
	result.size = stats.st_size;

	return result;
}

#if KYLA_PLATFORM_LINUX
struct LinuxFile final : public File
{
	LinuxFile (int fd, bool readOnly)
		: fd_ (fd)
		, readOnly_ (readOnly)
	{
	}

	~LinuxFile ()
	{
		if (fd_ != -1) {
			close (fd_);
		}
	}

	void CloseImpl () override
	{
		close (fd_);
		fd_ = -1;
	}

	void WriteImpl (const ArrayRef<>& buffer) override
	{
		write (fd_, buffer.GetData (), buffer.GetSize ());
	}

	std::int64_t ReadImpl (const MutableArrayRef<>& buffer) override
	{
		return read (fd_, buffer.GetData (), buffer.GetSize ());
	}

	void SeekImpl (const std::int64_t offset) override
	{
		lseek (fd_, offset, SEEK_SET);
	}

	void* MapImpl (const std::int64_t offset, const std::int64_t size) override
	{
		int protection = PROT_READ;
		if (! readOnly_) {
			protection |= PROT_WRITE;
		}

		auto r = mmap (nullptr, size,
			protection, MAP_SHARED, fd_, offset);

		mappings_ [r] = size;

		return r;
	}

	void UnmapImpl (void* p) override
	{
		munmap (p, mappings_.find (p)->second);
		mappings_.erase (p);
	}

	void SetSizeImpl (const std::int64_t size) override
	{
		ftruncate (fd_, size);
	}

	std::int64_t GetSizeImpl () const override
	{
		struct stat s;
		::fstat (fd_, &s);

		return s.st_size;
	}

	std::int64_t TellImpl () const override
	{
		return ::lseek (fd_, 0, SEEK_CUR);
	}

private:
	int fd_ = -1;
	bool readOnly_ = false;
	std::unordered_map<const void*, std::int64_t> mappings_;
};

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> CreateFile (const char* path, FileAccess access)
{
	///@TODO(minor) Handle access
	auto fd = open (path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	return std::unique_ptr<File> (new LinuxFile (fd, false /* read write */));
}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> CreateFile (const Path& path, FileAccess access)
{
	return CreateFile (path.c_str (), access);
}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> OpenFile (const char* path, FileAccess openMode,
	FileAccessHints)
{
	int mode;
	switch (openMode) {
	case FileAccess::Read:
		mode = O_RDONLY;
		break;

	case FileAccess::Write:
		mode = O_WRONLY;
		break;

	case FileAccess::ReadWrite:
		mode = O_RDWR;
		break;
	}

	auto fd = open (path, mode, S_IRUSR | S_IWUSR);
	return std::unique_ptr<File> (new LinuxFile (fd, openMode == FileAccess::Read));
}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> OpenFile (const Path& path, FileAccess openMode,
	FileAccessHints)
{
	return OpenFile (path.c_str (), openMode);
}
#elif KYLA_PLATFORM_WINDOWS
struct WindowsFile final : public File
{
	WindowsFile (HANDLE handle, int openMode)
		: fd_ (handle)
		, openMode_ (openMode)
	{
	}

	~WindowsFile ()
	{
		if (fd_ != INVALID_HANDLE_VALUE) {
			::CloseHandle (fd_);
		}
	}

	void CloseImpl () override
	{
		::CloseHandle (fd_);
		fd_ = INVALID_HANDLE_VALUE;
	}

	void WriteImpl (const ArrayRef<>& buffer) override
	{
		std::int64_t bytesLeft = buffer.GetSize ();
		std::int64_t bytesWritten = 0;

		while (bytesLeft > 0) {
			const DWORD bytesToWrite =
				static_cast<DWORD> (
					// This is in DWORD range
					std::min<std::int64_t> (std::numeric_limits<::DWORD>::max (),
						bytesLeft));

			::DWORD tmp = 0;

			const auto result = ::WriteFile (fd_,
				static_cast<const std::uint8_t*> (buffer.GetData ()) + bytesWritten,
				bytesToWrite,
				&tmp,
				nullptr);

			if (result == 0) {
				// Handle error
			} else {
				bytesWritten += tmp;
				bytesLeft -= tmp;
			}
		}
	}

	std::int64_t ReadImpl (const MutableArrayRef<>& buffer) override
	{
		std::int64_t bytesRead = 0;
		std::int64_t bytesLeft = buffer.GetSize ();

		while (bytesLeft > 0) {
			const DWORD bytesToRead =
				static_cast<DWORD> (
					// This is in DWORD range
					std::min<std::int64_t> (std::numeric_limits<::DWORD>::max (),
						bytesLeft));
			::DWORD tmp = 0;

			const ::BOOL ok = ::ReadFile (
				fd_,
				buffer.GetData (),
				static_cast<::DWORD> (bytesToRead),
				&tmp,
				NULL);

			if (!ok) {
				throw std::exception ("Error while reading file");
			}

			if (tmp == 0) {
				break;
			}

			bytesLeft -= tmp;
			bytesRead += tmp;
		}

		return bytesRead;
	}

	void SeekImpl (const std::int64_t offset) override
	{
		::LARGE_INTEGER pos = { 0 };
		pos.QuadPart = offset;
		::SetFilePointerEx (fd_, pos, NULL, FILE_BEGIN);
	}

	void* MapImpl (const std::int64_t offset, const std::int64_t size) override
	{
		const ::DWORD protection = ((openMode_ & GENERIC_WRITE) != 0) ? PAGE_READWRITE : PAGE_READONLY;
		const ::DWORD access = ((openMode_ & GENERIC_WRITE) != 0) ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

		// If set here, the file size will get set correctly
		const DWORD	sizeHigh = (offset + size) >> 32;
		const DWORD	sizeLow = (offset + size) & 0xFFFFFFFF;

		const ::HANDLE mapping = ::CreateFileMappingW (fd_, NULL,
			protection, sizeHigh, sizeLow, NULL);

		if (mapping == 0) {
			throw std::exception ("Error while mapping file");
		}

		// Offset must be page aligned
		void* const pointer = ::MapViewOfFile (mapping, access,
			static_cast<::DWORD>(offset >> 32),
			static_cast<::DWORD>(offset & 0xFFFFFFFF),
			size + offset);

		if (pointer == nullptr) {
			::CloseHandle (mapping);

			throw std::exception ("Error while creating map view.");
		}

		mappings_ [pointer] = mapping;

		return pointer;
	}

	void UnmapImpl (void* p) override
	{
		::UnmapViewOfFile (p);
		::CloseHandle (mappings_.find (p)->second);
		mappings_.erase (p);
	}

	void SetSizeImpl (const std::int64_t size) override
	{
		// Need to restore the file pointer
		const auto oldPosition = Tell ();
		Seek (size);

		const auto setOk = ::SetEndOfFile (fd_);
		Seek (oldPosition);

		// Documentation states non-zero is success
		// http://msdn.microsoft.com/en-us/library/windows/desktop/aa365531(v=vs.85).aspx
	}

	std::int64_t GetSizeImpl () const override
	{
		::LARGE_INTEGER size = { 0 };

		::GetFileSizeEx (fd_, &size);

		return size.QuadPart;
	}

	std::int64_t TellImpl () const override
	{
		LARGE_INTEGER position = { 0 };
		static const LARGE_INTEGER distance = { 0 };

		const auto result = ::SetFilePointerEx (fd_, distance,
			&position, FILE_CURRENT);

		if (result == 0) {
			throw std::exception ("Error while obtaining file pointer position.");
		}

		return position.QuadPart;
	}

private:
	HANDLE fd_ = INVALID_HANDLE_VALUE;
	int openMode_ = 0;
	std::unordered_map<const void*, HANDLE> mappings_;
};

///////////////////////////////////////////////////////////////////////////////
int ConvertFileAccess (const FileAccess openMode)
{
	switch (openMode) {
	case FileAccess::Read:
		return GENERIC_READ;

	case FileAccess::Write:
		return GENERIC_WRITE;

	case FileAccess::ReadWrite:
		return GENERIC_READ | GENERIC_WRITE;
	}

	return GENERIC_READ;
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> CreateFile (const char* path, FileAccess access)
{
	const auto mode = ConvertFileAccess (access);
	auto fd = ::CreateFileA (path, mode,
		0, nullptr, CREATE_ALWAYS, 0, 0);

	if (fd == INVALID_HANDLE_VALUE) {
		throw std::exception ("Could not create file");
	}

	return std::unique_ptr<File> (new WindowsFile (fd, mode));
}

///////////////////////////////////////////////////////////////////////////////
DWORD ConvertFileAccessHints (const FileAccessHints hints)
{
	switch (hints) {
	case FileAccessHints::None: 
		return 0;

	case FileAccessHints::SequentialScan:
		return FILE_FLAG_SEQUENTIAL_SCAN;

	default:
		return 0;
	}
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> OpenFile (const char* path, FileAccess openMode,
	FileAccessHints hints)
{
	const int mode = ConvertFileAccess (openMode);
	const int shareMode = openMode == FileAccess::Read ? FILE_SHARE_READ : 0;

	auto fd = ::CreateFileA (path, mode,
		shareMode, nullptr, OPEN_EXISTING, ConvertFileAccessHints (hints), 0);

	if (fd == INVALID_HANDLE_VALUE) {
		throw std::exception ("Could not open file");
	}

	return std::unique_ptr<File> (new WindowsFile (fd, mode));
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> CreateFile (const Path& path, FileAccess access)
{
	const auto mode = ConvertFileAccess (access);
	auto fd = ::CreateFileW (path.c_str (), mode,
		0, nullptr, CREATE_ALWAYS, 0, 0);

	if (fd == INVALID_HANDLE_VALUE) {
		throw std::exception ("Could not create file");
	}

	return std::unique_ptr<File> (new WindowsFile (fd, mode));
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> OpenFile (const Path& path, FileAccess openMode,
	FileAccessHints hints)
{
	const int mode = ConvertFileAccess (openMode);
	const int shareMode = openMode == FileAccess::Read ? FILE_SHARE_READ : 0;

	auto fd = ::CreateFileW (path.c_str (), mode,
		shareMode, nullptr, OPEN_EXISTING, ConvertFileAccessHints (hints), 0);

	if (fd == INVALID_HANDLE_VALUE) {
		throw std::exception ("Could not open file");
	}

	return std::unique_ptr<File> (new WindowsFile (fd, mode));
}
#else
#error Unsupported platform
#endif

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> CreateFile (const char* path)
{
	return CreateFile (path, FileAccess::ReadWrite);
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> CreateFile (const Path& path)
{
	return CreateFile (path, FileAccess::ReadWrite);
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> OpenFile (const char* path, FileAccess openMode)
{
	return OpenFile (path, openMode, FileAccessHints::None);
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<File> OpenFile (const Path& path, FileAccess openMode)
{
	return OpenFile (path, openMode, FileAccessHints::None);
}

///////////////////////////////////////////////////////////////////////////////
Path GetTemporaryFilename ()
{
#if KYLA_PLATFORM_WINDOWS
	char tempPathBuffer [MAX_PATH] = {0};
	if (GetTempPathA (MAX_PATH, tempPathBuffer) == 0) {
		///@TODO(minor) Handle error
	}

	char tempFileBuffer [MAX_PATH] = { 0 };
	if (GetTempFileNameA (tempPathBuffer, "kylatmp", 0, tempFileBuffer) == 0) {
		///@TODO(minor) Handle error
	}

	return Path{ tempFileBuffer };
#elif KYLA_PLATFORM_LINUX
	 char tempPathBuffer[L_tmpnam] = {0};
	 tmpnam(tempPathBuffer);
	 return Path{ tempPathBuffer };
#else
#error Unsupported platform
#endif
}
}
