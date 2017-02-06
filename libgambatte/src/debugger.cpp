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

#include "debugger.h"
#include "cpu.h"
#include "gambattepriv.h"

namespace gambatte {

unsigned GBDebugger::gbReadByte(unsigned address) {
	return p_->cpu.readByte(address);
}

void GBDebugger::gbWriteByte(unsigned address, unsigned data) {
	p_->cpu.writeByte(address, data);
}

void GBDebugger::stepIn() {
	p_->cpu.setEndCondition(END_ON_DESIRED_STACK);
	p_->cpu.setDesiredStack(-1);
}

void GBDebugger::stepOver() {
	p_->cpu.setEndCondition(END_ON_DESIRED_STACK);
	p_->cpu.setDesiredStack(0);
}

void GBDebugger::stepOut() {
	p_->cpu.setEndCondition(END_ON_DESIRED_STACK);
	p_->cpu.setDesiredStack(1);
}

CPURegisters GBDebugger::getRegisters() const {
	return p_->cpu.getRegisters();
}

void GBDebugger::setRegisters(const CPURegisters &newRegisters) {
	p_->cpu.setRegisters(newRegisters);
}

}
