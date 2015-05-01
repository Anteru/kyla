#ifndef KYLA_CORE_INTERNAL_FILEIO_H
#define KYLA_CORE_INTERNAL_FILEIO_H

#include <cstdint>
#include <memory>

namespace kyla {
struct File
{
	virtual ~File ();

	File ();
	File (const File&) = delete;
	File& operator= (const File&) = delete;

	void Write (const void* buffer, const std::int64_t size)
	{
		WriteImpl (buffer, size);
	}

	std::int64_t Read (void* buffer, const std::int64_t size)
	{
		return ReadImpl (buffer, size);
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
		MapImpl (offset, size);
	}

	void* Unmap (void* p)
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
	virtual void WriteImpl (const void* buffer, const std::int64_t size) = 0;
	virtual std::int64_t ReadImpl (void* buffer, const std::int64_t size) = 0;

	virtual void SeekImpl (const std::int64_t offset) = 0;
	virtual std::int64_t TellImpl () const = 0;
	virtual void* MapImpl (const std::int64_t offset, const std::int64_t size) = 0;
	virtual void* UnmapImpl (void* p) = 0;

	virtual void SetSizeImpl (const std::int64_t size) = 0;
	virtual std::int64_t GetSizeImpl () const = 0;

	virtual void CloseImpl () = 0;
};

enum class FileOpenMode
{
	Read,
	Write,
	ReadWrite
};

std::unique_ptr<File> OpenFile (const char* path, FileOpenMode openMode);
std::unique_ptr<File> CreateFile (const char* path);
}

#endif // FILEIO_H