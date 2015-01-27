#include "FileIO.h"

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

struct LinuxFile final : public File
{
	LinuxFile (int fd)
	: fd_ (fd)
	{
	}

	~LinuxFile ()
	{
		if (fd_ != -1) {
			close (fd_);
		}
	}

	void CloseImpl ()
	{
		close (fd_);
		fd_ = -1;
	}

	void WriteImpl (const void* buffer, const std::int64_t size) override
	{
		write (fd_, buffer, size);
	}

	std::int64_t ReadImpl (void* buffer, const std::int64_t size) override
	{
		return read (fd_, buffer, size);
	}

	void SeekImpl (const std::int64_t offset) override
	{
		lseek (fd_, offset, SEEK_SET);
	}

	void* MapImpl (const std::int64_t offset, const std::int64_t size) override
	{
		auto r = mmap (nullptr, size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd_, offset);

		mappings_ [r] = size;

		return r;
	}

	void* UnmapImpl (void* p) override
	{
		munmap (p, mappings_.find (p)->second);
		mappings_.erase (p);
	}

	void SetSizeImpl (const std::int64_t size) override
	{
		ftruncate (fd_, size);
	}

	std::int64_t GetSizeImpl () const
	{
		struct stat s;
		::fstat (fd_, &s);

		return s.st_size;
	}

private:
	int fd_ = -1;
	std::unordered_map<const void*, std::int64_t> mappings_;
};

std::unique_ptr<File> CreateFile (const char* path)
{
	auto fd = open (path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	return std::unique_ptr<File> (new LinuxFile (fd));
}

std::unique_ptr<File> OpenFile (const char* path)
{
	auto fd = open (path, O_RDWR, S_IRUSR | S_IWUSR);
	return std::unique_ptr<File> (new LinuxFile (fd));
}
}
