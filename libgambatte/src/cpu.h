//
//   Copyright (C) 2007 by sinamas <sinamas at users.sourceforge.net>
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

#ifndef CPU_H
#define CPU_H

#include "memory.h"
#include "cpuregisters.h"

namespace gambatte {

enum EndCondition { NORMAL_END = 0,					 // End only when cycles have elapsed.
					END_ON_BREAKPOINT = 1 << 0,		 // End when a breakpoint is reached.
					END_ON_DESIRED_STACK = 1 << 1 }; // End when desiredStack == 0. (for step over and step out)

class CPU {
public:
	CPU();
	long runFor(unsigned long cycles);
	void setStatePtrs(SaveState &state);
	void saveState(SaveState &state);
	void loadState(SaveState const &state);
	void loadSavedata() { mem_.loadSavedata(); }
	void saveSavedata() { mem_.saveSavedata(); }

	void setVideoBuffer(uint_least32_t *videoBuf, std::ptrdiff_t pitch) {
		mem_.setVideoBuffer(videoBuf, pitch);
	}

	void setInputGetter(InputGetter *getInput) {
		mem_.setInputGetter(getInput);
	}

	void setSaveDir(std::string const &sdir) {
		mem_.setSaveDir(sdir);
	}

	std::string const saveBasePath() const {
		return mem_.saveBasePath();
	}

	void setOsdElement(transfer_ptr<OsdElement> osdElement) {
		mem_.setOsdElement(osdElement);
	}

	LoadRes load(std::string const &romfile, bool forceDmg, bool multicartCompat) {
		return mem_.loadROM(romfile, forceDmg, multicartCompat);
	}

	bool loaded() const { return mem_.loaded(); }
	char const * romTitle() const { return mem_.romTitle(); }
	PakInfo const pakInfo(bool multicartCompat) const { return mem_.pakInfo(multicartCompat); }
	void setSoundBuffer(uint_least32_t *buf) { mem_.setSoundBuffer(buf); }
	std::size_t fillSoundBuffer() { return mem_.fillSoundBuffer(cycleCounter_); }
	bool isCgb() const { return mem_.isCgb(); }

	void setDmgPaletteColor(int palNum, int colorNum, unsigned long rgb32) {
		mem_.setDmgPaletteColor(palNum, colorNum, rgb32);
	}

	void setGameGenie(std::string const &codes) { mem_.setGameGenie(codes); }
	void setGameShark(std::string const &codes) { mem_.setGameShark(codes); }

	unsigned readByte(unsigned p) { return mem_.read(p, cycleCounter_); }
	void writeByte(unsigned p, unsigned data) { mem_.write(p, data, cycleCounter_); }
	
	unsigned getEndConditions() const { return endCondition_; }
	void setEndCondition(enum EndCondition endCondition) { endCondition_ |= endCondition; }
	void disableEndCondition(enum EndCondition endCondition) { endCondition_ &= ~endCondition; };
	void setDesiredStack(int desiredStack) { desiredStack_ = desiredStack; }

	CPURegisters getRegisters() const;
	void setRegisters(const CPURegisters &newRegisters);

private:
	Memory mem_;
	unsigned long cycleCounter_;
	unsigned short pc_;
	unsigned short sp;
	unsigned hf1, hf2, zf, cf;
	unsigned char a_, b, c, d, e, /*f,*/ h, l;
	bool skip_;
	
	unsigned endCondition_;
	int desiredStack_;
	bool skipBreakpoint_;

	void process(unsigned long cycles);
	bool shouldProcess();
};

}

#endif
