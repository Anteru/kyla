/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_FILEIO_H
#define KYLA_CORE_INTERNAL_FILEIO_H

#include <cstdint>
#include <memory>

#include <filesystem>

#include "ArrayRef.h"
#include <fmt/format.h>

namespace kyla {
using Path = std::filesystem::path;

struct File
{
	virtual ~File ();

	File ();
	File (const File&) = delete;
	File& operator= (const File&) = delete;

	void Write (const ArrayRef<>& data)
	{
		WriteImpl (data);
	}

	std::int64_t Read (const MutableArrayRef<>& buffer)
	{
		return ReadImpl (buffer);
	}

	void Seek (const std::int64_t offset)
	{
		SeekImpl (offset);
	}

	std::int64_t Tell () const
	{
		return TellImpl ();
	}

	void* Map (const std::int64_t offset, const std::int64_t size)
	{
		return MapImpl (offset, size);
	}

	void* Map ()
	{
		return MapImpl (0, GetSize ());
	}

	void Unmap (void* p)
	{
		UnmapImpl (p);
	}

	void SetSize (const std::int64_t size)
	{
		SetSizeImpl (size);
	}

	std::int64_t GetSize () const
	{
		return GetSizeImpl ();
	}

	void Close ()
	{
		CloseImpl ();
	}

private:
	virtual void WriteImpl (const ArrayRef<>& data) = 0;
	virtual std::int64_t ReadImpl (const MutableArrayRef<>& buffer) = 0;

	virtual void SeekImpl (const std::int64_t offset) = 0;
	virtual std::int64_t TellImpl () const = 0;
	virtual void* MapImpl (const std::int64_t offset, const std::int64_t size) = 0;
	virtual void UnmapImpl (void* p) = 0;

	virtual void SetSizeImpl (const std::int64_t size) = 0;
	virtual std::int64_t GetSizeImpl () const = 0;

	virtual void CloseImpl () = 0;
};

struct FileStat
{
	std::size_t size;
};

FileStat Stat (const Path& path);
FileStat Stat (const char* path);

enum class FileAccess
{
	Read,
	Write,
	ReadWrite
};

enum class FileAccessHints
{
	None,
	SequentialScan
};

std::unique_ptr<File> OpenFile (const char* path, FileAccess access);
std::unique_ptr<File> OpenFile (const char* path, FileAccess access,
	FileAccessHints hints);

std::unique_ptr<File> OpenFile (const Path& path, FileAccess access);
std::unique_ptr<File> OpenFile (const Path& path, FileAccess access,
	FileAccessHints hints);

std::unique_ptr<File> CreateFile (const char* path);
std::unique_ptr<File> CreateFile (const char* path, FileAccess access);
std::unique_ptr<File> CreateFile (const Path& path);
std::unique_ptr<File> CreateFile (const Path& path, FileAccess access);

Path GetTemporaryFilename ();
}

template <>
struct fmt::formatter<kyla::Path>
{
	constexpr auto parse (format_parse_context& ctx) {
		return ctx.end ();
	}

	template <typename FormatContext>
	auto format (const kyla::Path& p, FormatContext& ctx) {
		return format_to (ctx.out (), "{}", p.string());
	}
};
#endif // FILEIO_H
