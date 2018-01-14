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

#ifndef BOOTROM_H
#define BOOTROM_H

#include <string>

namespace gambatte {

class BootRom {
public:
	BootRom(const std::string &filename, size_t expectedSize);
	virtual ~BootRom();
	virtual bool isReadInBootRom(unsigned p) const;
	virtual unsigned read(unsigned p) const;
	virtual bool isEnabled() const;
	virtual void setEnabled(bool enabled);

private:
	unsigned char *romData;
	bool enabled = false;
	const size_t expectedSize;
};

class GBBootRom : public BootRom {
public:
	GBBootRom(const std::string &filename);
};

class GBCBootRom : public BootRom {
public:
	GBCBootRom(const std::string &filename);
	virtual bool isReadInBootRom(unsigned p) const;
};

}

#endif
