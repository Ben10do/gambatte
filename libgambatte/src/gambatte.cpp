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

#include "gambatte.h"
#include "cpu.h"
#include "initstate.h"
#include "savestate.h"
#include "statesaver.h"
#include "gambattepriv.h"
#include <cstring>
#include <sstream>
#include <climits>

#if CHAR_BIT != 8
#warning This version of Gambatte assumes that chars are 8 bits in size. \
Test thoroughly or use https://github.com/sinamas/gambatte.
#endif

using namespace std;

static string const statePath(string const &basePath, int stateNo) {
	return basePath + "_" + to_string(stateNo) + ".gqs";
}

namespace gambatte {

GB::GB() : p_(new Priv) {}

GB::~GB() {
	if (p_->cpu.loaded())
		p_->cpu.saveSavedata();

	delete p_;
}

ptrdiff_t GB::runFor(gambatte::uint_least32_t *const videoBuf, ptrdiff_t const pitch,
                          gambatte::uint_least32_t *const soundBuf, size_t &samples) {
	if (!p_->cpu.loaded()) {
		samples = 0;
		return -1;
	}

	p_->cpu.setVideoBuffer(videoBuf, pitch);
	p_->cpu.setSoundBuffer(soundBuf);

	long const cyclesSinceBlit = p_->cpu.runFor(samples * 2);
	samples = p_->cpu.fillSoundBuffer();
	return cyclesSinceBlit >= 0
	     ? static_cast<ptrdiff_t>(samples) - (cyclesSinceBlit >> 1)
	     : cyclesSinceBlit;
}

void GB::resetWithFlags(unsigned const flags, const bool saveSaveData) {
	p_->loadflags = flags;
    if (p_->cpu.loaded()) {
        if (saveSaveData) {
            p_->cpu.saveSavedata();
        }
        
        p_->cpu.resetMemorySize(flags & FORCE_DMG);
        p_->cpu.resetMbc(flags & MULTICART_COMPAT);
        reset(false);
    }
}

void GB::reset(const bool saveSaveData) {
	if (p_->cpu.loaded()) {
        if (saveSaveData) {
    		p_->cpu.saveSavedata();
        }

		SaveState state;
		p_->cpu.setStatePtrs(state);
		setInitState(state, p_->cpu.isCgb(), p_->loadflags & GBA_CGB, p_->cpu.isBootRomSet());
		p_->cpu.loadState(state);
		p_->cpu.loadSavedata();
	}
}

void GB::setInputGetter(InputGetter *getInput) {
	p_->cpu.setInputGetter(getInput);
}

void GB::setSaveDir(string const &sdir) {
	p_->cpu.setSaveDir(sdir);
}

LoadRes GB::load(string const &romfile, unsigned const flags) {
	if (p_->cpu.loaded())
		p_->cpu.saveSavedata();

	LoadRes const loadres = p_->cpu.load(romfile,
	                                     flags & FORCE_DMG,
	                                     flags & MULTICART_COMPAT);
	if (loadres == LOADRES_OK) {
		SaveState state;
		p_->cpu.setStatePtrs(state);
		p_->loadflags = flags;
		setInitState(state, p_->cpu.isCgb(), flags & GBA_CGB, p_->cpu.isBootRomSet());
		p_->cpu.loadState(state);
		p_->cpu.loadSavedata();

		p_->stateNo = 1;
	}

	return loadres;
}

bool GB::isCgb() const {
	return p_->cpu.isCgb();
}

bool GB::isLoaded() const {
	return p_->cpu.loaded();
}

void GB::saveSavedata() {
	if (p_->cpu.loaded())
		p_->cpu.saveSavedata();
}

void GB::setDmgPaletteColor(int palNum, int colorNum, unsigned long rgb32) {
	p_->cpu.setDmgPaletteColor(palNum, colorNum, rgb32);
}

bool GB::loadState(string const &filepath) {
	if (p_->cpu.loaded()) {
		p_->cpu.saveSavedata();

		SaveState state;
		p_->cpu.setStatePtrs(state);

		if (StateSaver::loadState(state, filepath)) {
			p_->cpu.loadState(state);
			return true;
		}
	}

	return false;
}

bool GB::saveState(gambatte::uint_least32_t const *videoBuf, ptrdiff_t pitch) {
	return saveState(videoBuf, pitch, statePath(p_->cpu.saveBasePath(), p_->stateNo));
}

bool GB::loadState() {
	return loadState(statePath(p_->cpu.saveBasePath(), p_->stateNo));
}

bool GB::saveState(gambatte::uint_least32_t const *videoBuf, ptrdiff_t pitch, string const &filepath) {
	if (p_->cpu.loaded()) {
		SaveState state;
		p_->cpu.setStatePtrs(state);
		p_->cpu.saveState(state);
		return StateSaver::saveState(state, videoBuf, pitch, filepath);
	}

	return false;
}

void GB::selectState(int n) {
	p_->stateNo = abs(n % 10);
}

int GB::currentState() const { return p_->stateNo; }

string const GB::romTitle() const {
	if (p_->cpu.loaded()) {
		char title[0x11];
		memcpy(title, p_->cpu.romTitle(), 0x10);
		title[title[0xF] & 0x80 ? 0xF : 0x10] = '\0';
		return string(title);
	}

	return "";
}

PakInfo const GB::pakInfo() const { return p_->cpu.pakInfo(p_->loadflags & MULTICART_COMPAT); }

void GB::setGameGenie(string const &codes) {
	p_->cpu.setGameGenie(codes);
}

void GB::setGameShark(string const &codes) {
	p_->cpu.setGameShark(codes);
}

bool GB::setDmgBootRom(const std::string &path) {
	bool wasEnabled = !p_->cpu.isCgb() && p_->cpu.isBootRomEnabled();
	try {
		p_->cpu.setGBBootRom(path);

		if (wasEnabled) {
			reset();
		}
		return true;

	} catch (exception) {
		return false;
	}
}

bool GB::setCgbBootRom(const std::string &path) {
	bool wasEnabled = p_->cpu.isCgb() && p_->cpu.isBootRomEnabled();
	try {
		p_->cpu.setGBCBootRom(path);

		if (wasEnabled) {
			reset();
		}
		return true;

	} catch (exception) {
		return false;
	}
}

}
