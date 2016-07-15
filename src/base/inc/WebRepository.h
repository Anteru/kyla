/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_WEB_REPOSITORY_H
#define KYLA_CORE_INTERNAL_WEB_REPOSITORY_H

#include "BaseRepository.h"
#include "sql/Database.h"

namespace kyla {
class WebRepository final : public BaseRepository
{
public:
	WebRepository (const std::string& path);
	~WebRepository ();

private:
	void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) override;

	Sql::Database& GetDatabaseImpl () override;

	Sql::Database db_;
	Path dbPath_;
	std::string url_;

	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace kyla

#endif
