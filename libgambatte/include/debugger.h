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

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "gambatte.h"
#include "cpuregisters.h"

namespace gambatte {

class GBDebugger : public GB {
public:
	/**
	 * Read the byte at p, as though it was read by the Game Boy.
	 * This means that the read will be affected by time-dependent
	 * inaccessibility (e.g. for VRAM, SRAM, wave data, etc.)
	 */
	unsigned gbReadByte(unsigned address);

	/**
	 * Write the byte 'data' to p, as though it was written by the
	 * Game Boy. This means that the write will be affected by
	 * time-dependent inaccessibility (e.g. for VRAM, SRAM,
	 * wave data, etc.), and effects such as bank changes may occur.
	 */
	void gbWriteByte(unsigned address, unsigned data);

	/**
	 * On the next call to runFor(), emulate until the next instruction
	 * has been completed. This may cause emulation to finish inside a
	 * just-called subroutine.
	 */
	void stepIn();

   	/**
   	 * On the next call to runFor(), emulate until the next instruction
	 * has been completed, or if the current instruction is a call,
	 * emulates until the subroutine has been returned from.
   	 */
	void stepOver();

	/**
	 * On the next call to runFor(), emulate until the current subroutine
	 * has been returned from.
	 */
	void stepOut();

	/**
	 * Get the current value of the CPU's registers.
	 */
	CPURegisters getRegisters() const;
	
	/**
	 * Set the CPU's registers to the given values.
	 */
	void setRegisters(const CPURegisters &newRegisters);

};

}

#endif
