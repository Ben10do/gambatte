//
//   Copyright (C) 2017 by Ben10do <Ben10do@users.noreply.github.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License version 2 for more details.
//
//   You should have received a copy of the GNU General Public License
//   version 2 along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
//

#include "../common/scoped_ptr.h"
#include "../file/file.h"
#include "bootrom.h"
#include <ios>

using namespace std;

namespace gambatte {

BootRom::BootRom(const string &filename, size_t expectedSize_)
: expectedSize(expectedSize_) {
	const scoped_ptr<File> file(newFileInstance(filename));

	if (file->fail()) {
		throw ios_base::failure("Failed to load Boot ROM");
	}

	if (file->size() < expectedSize) {
		throw ios_base::failure("The Boot ROM is too small.");
	}

	romData = new unsigned char[expectedSize];
	file->read(reinterpret_cast<char *>(romData), expectedSize);
}

BootRom::~BootRom() {
	delete []romData;
}

unsigned BootRom::read(unsigned p) const {
	return romData[p];
}

bool BootRom::isEnabled() const {
	return enabled;
}

void BootRom::setEnabled(bool enabled) {
	this->enabled = enabled;
}

bool BootRom::isReadInBootRom(unsigned p) const {
	return p < expectedSize;
}

GBBootRom::GBBootRom(const string &filename)
: BootRom(filename, 0x100) {

}

GBCBootRom::GBCBootRom(const string &filename)
: BootRom(filename, 0x900) {

}

bool GBCBootRom::isReadInBootRom(unsigned p) const {
	return (p < 0x100 || p >= 0x200) && BootRom::isReadInBootRom(p);
}

}
