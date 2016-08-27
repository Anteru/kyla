/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "WebRepository.h"

#include "sql/Database.h"
#include "Exception.h"
#include "Log.h"

#include <boost/format.hpp>

#if KYLA_PLATFORM_WINDOWS
#pragma comment(lib, "wininet.lib")

#include <Windows.h>
#include <Wininet.h>

#undef min
#undef max
#undef CreateFile
#endif

namespace kyla {
struct WebRepository::Impl
{
#if KYLA_PLATFORM_WINDOWS
	Impl ()
	{
		internet_ = InternetOpen ("kyla",
			INTERNET_OPEN_TYPE_DIRECT,
			NULL,
			NULL,
			0);
	}

	struct File
	{
	public:
		File (const File&) = delete;
		File& operator= (const File&) = delete;

		File (HINTERNET internet, const std::string& url)
		{
			handle_ = InternetOpenUrl (internet,
				url.c_str (),
				NULL, /* headers */
				0, /* header length */
				0, /* flags */
				NULL /* context */);
		}

		~File ()
		{
			InternetCloseHandle (handle_);
		}
		
		int64 Read (const MutableArrayRef<>& buffer)
		{
			int64 readTotal = 0;
			DWORD read = 0;

			while (readTotal < buffer.GetSize ()) {
				DWORD toRead = static_cast<DWORD> (
					std::min<int64> (buffer.GetSize () - readTotal, 
					std::numeric_limits<DWORD>::max ()));
				InternetReadFile (handle_,
					buffer.GetData (), toRead, &read);
				readTotal += read;

				if (read == 0) {
					break;
				}
			}

			return readTotal;
		}

		void Seek (int64 offset)
		{
			///@TODO(minor) Optimize this to not seek unless needed
			LONG upperBits = offset >> 32;
			InternetSetFilePointer (handle_, offset & 0xFFFFFFFF,
				&upperBits, FILE_BEGIN, NULL);
		}

		HINTERNET handle_;
	};

	std::unique_ptr<File> Open (const std::string& file)
	{
		return std::unique_ptr<File> (new File{ internet_, file });
	}

	~Impl ()
	{
		InternetCloseHandle (internet_);
	}

	HINTERNET internet_;
#endif
};

///////////////////////////////////////////////////////////////////////////////
WebRepository::WebRepository (const std::string& path)
	: impl_ (new Impl)
{
	// path must end with '/'
	if (path.back () != '/') {
		throw RuntimeException (str (
			boost::format ("Web repository url must end with '/' (got: '%1%')") % path),
			KYLA_FILE_LINE);
	}
	const auto dbWebFile = impl_->Open (std::string (path) + "repository.db");
	url_ = path;
	dbPath_ = GetTemporaryFilename ();

	// Extra scope so it's closed by the time we try to open
	{
		auto dbLocalFile = CreateFile (dbPath_);
		std::vector<byte> buffer;
		buffer.resize (1 << 20); // 1 MiB

		for (;;) {
			const auto bytesRead = dbWebFile->Read (buffer);
		
			if (bytesRead == 0) {
				break;
			}
		
			dbLocalFile->Write (ArrayRef<byte> {buffer}.Slice (0, bytesRead));
		}
	}

	db_ = Sql::Database::Open (dbPath_);
}

///////////////////////////////////////////////////////////////////////////////
WebRepository::~WebRepository ()
{
	db_.Close ();
	boost::filesystem::remove (dbPath_);
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& WebRepository::GetDatabaseImpl ()
{
	return db_;
}

namespace {
	struct WebPackageFile final : public PackedRepositoryBase::PackageFile
	{
	public:
		WebPackageFile (std::unique_ptr<WebRepository::Impl::File>&& file)
			: file_ (std::move (file))
		{
		}

		bool Read (const int64 offset, const MutableArrayRef<>& buffer) override
		{
			file_->Seek (offset);
			return file_->Read (buffer) == buffer.GetSize ();
		}

	private:
		std::unique_ptr<WebRepository::Impl::File> file_;
	};
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<PackedRepositoryBase::PackageFile> WebRepository::OpenPackage (const std::string& packageName) const
{
	return std::unique_ptr<PackageFile> { new WebPackageFile{
		impl_->Open (url_ + packageName)
	}};
}
} // namespace kyla
