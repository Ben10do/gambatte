//
//   Copyright (C) 2008 by sinamas <sinamas at users.sourceforge.net>
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

#include "statesaver.h"
#include "savestate.h"
#include "array.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <vector>
#include <cstring>

namespace {

using namespace gambatte;

struct Saver {
	char const *label;
	void (*save)(std::ofstream &file, SaveState const &state);
	void (*load)(std::ifstream &file, SaveState &state);
	std::size_t labelsize;
};

static inline bool operator<(Saver const &l, Saver const &r) {
	return std::strcmp(l.label, r.label) < 0;
}

static void put24(std::ofstream &file, unsigned long data) {
	file.put(data >> 16 & 0xFF);
	file.put(data >>  8 & 0xFF);
	file.put(data       & 0xFF);
}

static void put32(std::ofstream &file, unsigned long data) {
	file.put(data >> 24 & 0xFF);
	file.put(data >> 16 & 0xFF);
	file.put(data >>  8 & 0xFF);
	file.put(data       & 0xFF);
}

static void write(std::ofstream &file, unsigned char data) {
	static char const inf[] = { 0x00, 0x00, 0x01 };
	file.write(inf, sizeof inf);
	file.put(data & 0xFF);
}

static void write(std::ofstream &file, unsigned short data) {
	static char const inf[] = { 0x00, 0x00, 0x02 };
	file.write(inf, sizeof inf);
	file.put(data >> 8 & 0xFF);
	file.put(data      & 0xFF);
}

static void write(std::ofstream &file, unsigned long data) {
	static char const inf[] = { 0x00, 0x00, 0x04 };
	file.write(inf, sizeof inf);
	put32(file, data);
}

static void write(std::ofstream &file, unsigned char const *data, std::size_t size) {
	put24(file, size);
	file.write(reinterpret_cast<char const *>(data), size);
}

static void write(std::ofstream &file, bool const *data, std::size_t size) {
	put24(file, size);
	std::for_each(data, data + size,
		std::bind1st(std::mem_fun(&std::ofstream::put), &file));
}

static unsigned long get24(std::ifstream &file) {
	unsigned long tmp = file.get() & 0xFF;
	tmp =   tmp << 8 | (file.get() & 0xFF);
	return  tmp << 8 | (file.get() & 0xFF);
}

static unsigned long read(std::ifstream &file) {
	unsigned long size = get24(file);
	if (size > 4) {
		file.ignore(size - 4);
		size = 4;
	}

	unsigned long out = 0;
	switch (size) {
	case 4: out = (out | (file.get() & 0xFF)) << 8;
	case 3: out = (out | (file.get() & 0xFF)) << 8;
	case 2: out = (out | (file.get() & 0xFF)) << 8;
	case 1: out =  out | (file.get() & 0xFF);
	}

	return out;
}

static inline void read(std::ifstream &file, unsigned char &data) {
	data = read(file) & 0xFF;
}

static inline void read(std::ifstream &file, unsigned short &data) {
	data = read(file) & 0xFFFF;
}

static inline void read(std::ifstream &file, unsigned long &data) {
	data = read(file);
}

static void read(std::ifstream &file, unsigned char *buf, std::size_t bufsize) {
	std::size_t const size = get24(file);
	std::size_t const minsize = std::min(size, bufsize);
	file.read(reinterpret_cast<char*>(buf), minsize);
	file.ignore(size - minsize);
}

static void read(std::ifstream &file, bool *buf, std::size_t bufsize) {
	std::size_t const size = get24(file);
	std::size_t const minsize = std::min(size, bufsize);
	for (std::size_t i = 0; i < minsize; ++i)
		buf[i] = file.get();

	file.ignore(size - minsize);
}

} // anon namespace

namespace gambatte {

class SaverList {
public:
	typedef std::vector<Saver> list_t;
	typedef list_t::const_iterator const_iterator;

	SaverList();
	const_iterator begin() const { return list.begin(); }
	const_iterator end() const { return list.end(); }
	std::size_t maxLabelsize() const { return maxLabelsize_; }

private:
	list_t list;
	std::size_t maxLabelsize_;
};

static void pushSaver(SaverList::list_t &list, char const *label,
		void (*save)(std::ofstream &file, SaveState const &state),
		void (*load)(std::ifstream &file, SaveState &state),
		std::size_t labelsize) {
	Saver saver = { label, save, load, labelsize };
	list.push_back(saver);
}

SaverList::SaverList() {
#define ADD(arg) do { \
	struct Func { \
		static void save(std::ofstream &file, SaveState const &state) { write(file, state.arg); } \
		static void load(std::ifstream &file, SaveState &state) { read(file, state.arg); } \
	}; \
	pushSaver(list, label, Func::save, Func::load, sizeof label); \
} while (0)

#define ADDPTR(arg) do { \
	struct Func { \
		static void save(std::ofstream &file, SaveState const &state) { \
			write(file, state.arg.get(), state.arg.size()); \
		} \
		static void load(std::ifstream &file, SaveState &state) { \
			read(file, state.arg.ptr, state.arg.size()); \
		} \
	}; \
	pushSaver(list, label, Func::save, Func::load, sizeof label); \
} while (0)

#define ADDARRAY(arg) do { \
	struct Func { \
		static void save(std::ofstream &file, SaveState const &state) { \
			write(file, state.arg, sizeof state.arg); \
		} \
		static void load(std::ifstream &file, SaveState &state) { \
			read(file, state.arg, sizeof state.arg); \
		} \
	}; \
	pushSaver(list, label, Func::save, Func::load, sizeof label); \
} while (0)

	{ static char const label[] = "cc";      ADD(cpu.cycleCounter); }
	{ static char const label[] = "pc";      ADD(cpu.pc); }
	{ static char const label[] = "sp";      ADD(cpu.sp); }
	{ static char const label[] = "a";       ADD(cpu.a); }
	{ static char const label[] = "b";       ADD(cpu.b); }
	{ static char const label[] = "c";       ADD(cpu.c); }
	{ static char const label[] = "d";       ADD(cpu.d); }
	{ static char const label[] = "e";       ADD(cpu.e); }
	{ static char const label[] = "f";       ADD(cpu.f); }
	{ static char const label[] = "h";       ADD(cpu.h); }
	{ static char const label[] = "l";       ADD(cpu.l); }
	{ static char const label[] = "skip";    ADD(cpu.skip); }
	{ static char const label[] = "hang";    ADD(cpu.hang); }
	{ static char const label[] = "halt";    ADD(mem.halted); }
	{ static char const label[] = "vram";    ADDPTR(mem.vram); }
	{ static char const label[] = "sram";    ADDPTR(mem.sram); }
	{ static char const label[] = "wram";    ADDPTR(mem.wram); }
	{ static char const label[] = "hram";    ADDPTR(mem.ioamhram); }
	{ static char const label[] = "ldivup";  ADD(mem.divLastUpdate); }
	{ static char const label[] = "ltimaup"; ADD(mem.timaLastUpdate); }
	{ static char const label[] = "tmatime"; ADD(mem.tmatime); }
	{ static char const label[] = "serialt"; ADD(mem.nextSerialtime); }
	{ static char const label[] = "lodmaup"; ADD(mem.lastOamDmaUpdate); }
	{ static char const label[] = "minintt"; ADD(mem.minIntTime); }
	{ static char const label[] = "unhaltt"; ADD(mem.unhaltTime); }
	{ static char const label[] = "rombank"; ADD(mem.rombank); }
	{ static char const label[] = "dmasrc";  ADD(mem.dmaSource); }
	{ static char const label[] = "dmadst";  ADD(mem.dmaDestination); }
	{ static char const label[] = "rambank"; ADD(mem.rambank); }
	{ static char const label[] = "odmapos"; ADD(mem.oamDmaPos); }
	{ static char const label[] = "ime";     ADD(mem.IME); }
	{ static char const label[] = "sramon";  ADD(mem.enableRam); }
	{ static char const label[] = "rambmod"; ADD(mem.rambankMode); }
	{ static char const label[] = "hdma";    ADD(mem.hdmaTransfer); }
	{ static char const label[] = "bgp";     ADDPTR(ppu.bgpData); }
	{ static char const label[] = "objp";    ADDPTR(ppu.objpData); }
	{ static char const label[] = "sposbuf"; ADDPTR(ppu.oamReaderBuf); }
	{ static char const label[] = "spszbuf"; ADDPTR(ppu.oamReaderSzbuf); }
	{ static char const label[] = "spattr";  ADDARRAY(ppu.spAttribList); }
	{ static char const label[] = "spbyte0"; ADDARRAY(ppu.spByte0List); }
	{ static char const label[] = "spbyte1"; ADDARRAY(ppu.spByte1List); }
	{ static char const label[] = "vcycles"; ADD(ppu.videoCycles); }
	{ static char const label[] = "edM0tim"; ADD(ppu.enableDisplayM0Time); }
	{ static char const label[] = "m0time";  ADD(ppu.lastM0Time); }
	{ static char const label[] = "nm0irq";  ADD(ppu.nextM0Irq); }
	{ static char const label[] = "bgtw";    ADD(ppu.tileword); }
	{ static char const label[] = "bgntw";   ADD(ppu.ntileword); }
	{ static char const label[] = "winypos"; ADD(ppu.winYPos); }
	{ static char const label[] = "xpos";    ADD(ppu.xpos); }
	{ static char const label[] = "endx";    ADD(ppu.endx); }
	{ static char const label[] = "ppur0";   ADD(ppu.reg0); }
	{ static char const label[] = "ppur1";   ADD(ppu.reg1); }
	{ static char const label[] = "bgatrb";  ADD(ppu.attrib); }
	{ static char const label[] = "bgnatrb"; ADD(ppu.nattrib); }
	{ static char const label[] = "ppustat"; ADD(ppu.state); }
	{ static char const label[] = "nsprite"; ADD(ppu.nextSprite); }
	{ static char const label[] = "csprite"; ADD(ppu.currentSprite); }
	{ static char const label[] = "lyc";     ADD(ppu.lyc); }
	{ static char const label[] = "m0lyc";   ADD(ppu.m0lyc); }
	{ static char const label[] = "oldwy";   ADD(ppu.oldWy); }
	{ static char const label[] = "windraw"; ADD(ppu.winDrawState); }
	{ static char const label[] = "wscx";    ADD(ppu.wscx); }
	{ static char const label[] = "wemastr"; ADD(ppu.weMaster); }
	{ static char const label[] = "lcdsirq"; ADD(ppu.pendingLcdstatIrq); }
	{ static char const label[] = "spucntr"; ADD(spu.cycleCounter); }
	{ static char const label[] = "swpcntr"; ADD(spu.ch1.sweep.counter); }
	{ static char const label[] = "swpshdw"; ADD(spu.ch1.sweep.shadow); }
	{ static char const label[] = "swpneg";  ADD(spu.ch1.sweep.negging); }
	{ static char const label[] = "dut1ctr"; ADD(spu.ch1.duty.nextPosUpdate); }
	{ static char const label[] = "dut1pos"; ADD(spu.ch1.duty.pos); }
	{ static char const label[] = "dut1hi";  ADD(spu.ch1.duty.high); }
	{ static char const label[] = "env1ctr"; ADD(spu.ch1.env.counter); }
	{ static char const label[] = "env1vol"; ADD(spu.ch1.env.volume); }
	{ static char const label[] = "len1ctr"; ADD(spu.ch1.lcounter.counter); }
	{ static char const label[] = "len1val"; ADD(spu.ch1.lcounter.lengthCounter); }
	{ static char const label[] = "nr10";    ADD(spu.ch1.sweep.nr0); }
	{ static char const label[] = "nr13";    ADD(spu.ch1.duty.nr3); }
	{ static char const label[] = "nr14";    ADD(spu.ch1.nr4); }
	{ static char const label[] = "c1mastr"; ADD(spu.ch1.master); }
	{ static char const label[] = "dut2ctr"; ADD(spu.ch2.duty.nextPosUpdate); }
	{ static char const label[] = "dut2pos"; ADD(spu.ch2.duty.pos); }
	{ static char const label[] = "dut2hi";  ADD(spu.ch2.duty.high); }
	{ static char const label[] = "env2ctr"; ADD(spu.ch2.env.counter); }
	{ static char const label[] = "env2vol"; ADD(spu.ch2.env.volume); }
	{ static char const label[] = "len2ctr"; ADD(spu.ch2.lcounter.counter); }
	{ static char const label[] = "len2val"; ADD(spu.ch2.lcounter.lengthCounter); }
	{ static char const label[] = "nr23";    ADD(spu.ch2.duty.nr3); }
	{ static char const label[] = "nr24";    ADD(spu.ch2.nr4); }
	{ static char const label[] = "c2mastr"; ADD(spu.ch2.master); }
	{ static char const label[] = "waveram"; ADDPTR(spu.ch3.waveRam); }
	{ static char const label[] = "len3ctr"; ADD(spu.ch3.lcounter.counter); }
	{ static char const label[] = "len3val"; ADD(spu.ch3.lcounter.lengthCounter); }
	{ static char const label[] = "wavectr"; ADD(spu.ch3.waveCounter); }
	{ static char const label[] = "lwavrdt"; ADD(spu.ch3.lastReadTime); }
	{ static char const label[] = "wavepos"; ADD(spu.ch3.wavePos); }
	{ static char const label[] = "wavsmpl"; ADD(spu.ch3.sampleBuf); }
	{ static char const label[] = "nr33";    ADD(spu.ch3.nr3); }
	{ static char const label[] = "nr34";    ADD(spu.ch3.nr4); }
	{ static char const label[] = "c3mastr"; ADD(spu.ch3.master); }
	{ static char const label[] = "lfsrctr"; ADD(spu.ch4.lfsr.counter); }
	{ static char const label[] = "lfsrreg"; ADD(spu.ch4.lfsr.reg); }
	{ static char const label[] = "env4ctr"; ADD(spu.ch4.env.counter); }
	{ static char const label[] = "env4vol"; ADD(spu.ch4.env.volume); }
	{ static char const label[] = "len4ctr"; ADD(spu.ch4.lcounter.counter); }
	{ static char const label[] = "len4val"; ADD(spu.ch4.lcounter.lengthCounter); }
	{ static char const label[] = "nr44";    ADD(spu.ch4.nr4); }
	{ static char const label[] = "c4mastr"; ADD(spu.ch4.master); }
	{ static char const label[] = "rtcbase"; ADD(rtc.baseTime); }
	{ static char const label[] = "rtchalt"; ADD(rtc.haltTime); }
	{ static char const label[] = "rtcdh";   ADD(rtc.dataDh); }
	{ static char const label[] = "rtcdl";   ADD(rtc.dataDl); }
	{ static char const label[] = "rtch";    ADD(rtc.dataH); }
	{ static char const label[] = "rtcm";    ADD(rtc.dataM); }
	{ static char const label[] = "rtcs";    ADD(rtc.dataS); }
	{ static char const label[] = "rtclld";  ADD(rtc.lastLatchData); }

#undef ADD
#undef ADDPTR
#undef ADDARRAY

	list.resize(list.size());
	std::sort(list.begin(), list.end());

	maxLabelsize_ = 0;

	for (std::size_t i = 0; i < list.size(); ++i) {
		if (list[i].labelsize > maxLabelsize_)
			maxLabelsize_ = list[i].labelsize;
	}
}

}

namespace {

static void writeSnapShot(std::ofstream &file, uint_least32_t const *pixels, std::ptrdiff_t const pitch) {
	put24(file, pixels ? StateSaver::ss_width * StateSaver::ss_height * sizeof(uint_least32_t) : 0);

	if (pixels) {
		for (unsigned h = StateSaver::ss_height; h--;) {
			file.write(reinterpret_cast<char const *>(pixels), StateSaver::ss_width * sizeof(uint_least32_t));
			pixels += pitch;
		}
	}
}

static SaverList list;

} // anon namespace

namespace gambatte {

bool StateSaver::saveState(SaveState const &state,
		uint_least32_t const *const videoBuf,
		std::ptrdiff_t const pitch, std::string const &filename) {
	std::ofstream file(filename.c_str(), std::ios_base::binary);
	if (!file)
		return false;

	{ static char const ver[] = { 0, 2 }; file.write(ver, sizeof ver); }
	writeSnapShot(file, videoBuf, pitch);

	for (SaverList::const_iterator it = list.begin(); it != list.end(); ++it) {
		file.write(it->label, it->labelsize);
		(*it->save)(file, state);
	}

	return !file.fail();
}

bool StateSaver::loadState(SaveState &state, std::string const &filename) {
	std::ifstream file(filename.c_str(), std::ios_base::binary);
	if (!file || file.get() != 0)
		return false;

	file.ignore();
	file.ignore(get24(file));

	Array<char> const labelbuf(list.maxLabelsize());
	Saver const labelbufSaver = { labelbuf, 0, 0, list.maxLabelsize() };
	SaverList::const_iterator done = list.begin();

	while (file.good() && done != list.end()) {
		file.getline(labelbuf, list.maxLabelsize(), '\0');

		SaverList::const_iterator it = done;
		if (std::strcmp(labelbuf, it->label)) {
			it = std::lower_bound(it + 1, list.end(), labelbufSaver);

			if (it == list.end() || std::strcmp(labelbuf, it->label)) {
				file.ignore(get24(file));
				continue;
			}
		} else
			++done;

		(*it->load)(file, state);
	}

	state.cpu.cycleCounter &= 0x7FFFFFFF;
	state.spu.cycleCounter &= 0x7FFFFFFF;

	return true;
}

}
