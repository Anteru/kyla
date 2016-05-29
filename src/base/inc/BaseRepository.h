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

#ifndef KYLA_CORE_INTERNAL_BASE_REPOSITORY_H
#define KYLA_CORE_INTERNAL_BASE_REPOSITORY_H

#include "Repository.h"

namespace kyla {
class BaseRepository : public Repository
{
public:
	virtual ~BaseRepository () = default;

private:
	std::vector<FilesetInfo> GetFilesetInfosImpl () override;
	std::string GetFilesetNameImpl (const Uuid& filesetId) override;

	void RepairImpl (Repository& source) override;
	void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& filesets,
		Log& log, Progress& progress) override;
};
}

#endif
