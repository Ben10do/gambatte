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

using namespace std;

namespace {

using namespace gambatte;

enum AsciiChar {
	NUL, SOH, STX, ETX, EOT, ENQ, ACK, BEL,  BS, TAB,  LF,  VT,  FF,  CR,  SO,  SI,
	DLE, DC1, DC2, DC3, DC4, NAK, SYN, ETB, CAN,  EM, SUB, ESC,  FS,  GS,  RS,  US,
	 SP, XCL, QOT, HSH, DLR, PRC, AMP, APO, LPA, RPA, AST, PLU, COM, HYP, STP, DIV,
	NO0, NO1, NO2, NO3, NO4, NO5, NO6, NO7, NO8, NO9, CLN, SCL,  LT, EQL,  GT, QTN,
	 AT,   A,   B,   C,   D,   E,   F,   G,   H,   I,   J,   K,   L,   M,   N,   O,
	  P,   Q,   R,   S,   T,   U,   V,   W,   X,   Y,   Z, LBX, BSL, RBX, CAT, UND,
	ACN,   a,   b,   c,   d,   e,   f,   g,   h,   i,   j,   k,   l,   m,   n,   o,
	  p,   q,   r,   s,   t,   u,   v,   w,   x,   y,   z, LBR, BAR, RBR, TLD, DEL
};

struct Saver {
	char const *label;
	void (*save)(ofstream &file, SaveState const &state);
	void (*load)(ifstream &file, SaveState &state);
	size_t labelsize;
};

static inline bool operator<(Saver const &l, Saver const &r) {
	return strcmp(l.label, r.label) < 0;
}

static void put24(ofstream &file, unsigned long data) {
	file.put(data >> 16 & 0xFF);
	file.put(data >>  8 & 0xFF);
	file.put(data       & 0xFF);
}

static void put32(ofstream &file, unsigned long data) {
	file.put(data >> 24 & 0xFF);
	file.put(data >> 16 & 0xFF);
	file.put(data >>  8 & 0xFF);
	file.put(data       & 0xFF);
}

static void write(ofstream &file, unsigned char data) {
	static char const inf[] = { 0x00, 0x00, 0x01 };
	file.write(inf, sizeof inf);
	file.put(data & 0xFF);
}

static void write(ofstream &file, unsigned short data) {
	static char const inf[] = { 0x00, 0x00, 0x02 };
	file.write(inf, sizeof inf);
	file.put(data >> 8 & 0xFF);
	file.put(data      & 0xFF);
}

static void write(ofstream &file, unsigned long data) {
	static char const inf[] = { 0x00, 0x00, 0x04 };
	file.write(inf, sizeof inf);
	put32(file, data);
}

static void write(ofstream &file, unsigned char const *data, size_t size) {
	put24(file, size);
	file.write(reinterpret_cast<char const *>(data), size);
}

static void write(ofstream &file, bool const *data, size_t size) {
	put24(file, size);
	for_each(data, data + size,
		bind1st(mem_fun(&ofstream::put), &file));
}

static unsigned long get24(ifstream &file) {
	unsigned long tmp = file.get() & 0xFF;
	tmp =   tmp << 8 | (file.get() & 0xFF);
	return  tmp << 8 | (file.get() & 0xFF);
}

static unsigned long read(ifstream &file) {
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

static inline void read(ifstream &file, unsigned char &data) {
	data = read(file) & 0xFF;
}

static inline void read(ifstream &file, unsigned short &data) {
	data = read(file) & 0xFFFF;
}

static inline void read(ifstream &file, unsigned long &data) {
	data = read(file);
}

static void read(ifstream &file, unsigned char *buf, size_t bufsize) {
	size_t const size = get24(file);
	size_t const minsize = min(size, bufsize);
	file.read(reinterpret_cast<char*>(buf), minsize);
	file.ignore(size - minsize);
}

static void read(ifstream &file, bool *buf, size_t bufsize) {
	size_t const size = get24(file);
	size_t const minsize = min(size, bufsize);
	for (size_t i = 0; i < minsize; ++i)
		buf[i] = file.get();

	file.ignore(size - minsize);
}

} // anon namespace

namespace gambatte {

class SaverList {
public:
	typedef vector<Saver> list_t;
	typedef list_t::const_iterator const_iterator;

	SaverList();
	const_iterator begin() const { return list.begin(); }
	const_iterator end() const { return list.end(); }
	size_t maxLabelsize() const { return maxLabelsize_; }

private:
	list_t list;
	size_t maxLabelsize_;
};

static void pushSaver(SaverList::list_t &list, char const *label,
		void (*save)(ofstream &file, SaveState const &state),
		void (*load)(ifstream &file, SaveState &state),
		size_t labelsize) {
	Saver saver = { label, save, load, labelsize };
	list.push_back(saver);
}

SaverList::SaverList() {
#define ADD(arg) do { \
	struct Func { \
		static void save(ofstream &file, SaveState const &state) { write(file, state.arg); } \
		static void load(ifstream &file, SaveState &state) { read(file, state.arg); } \
	}; \
	pushSaver(list, label, Func::save, Func::load, sizeof label); \
} while (0)

#define ADDPTR(arg) do { \
	struct Func { \
		static void save(ofstream &file, SaveState const &state) { \
			write(file, state.arg.get(), state.arg.size()); \
		} \
		static void load(ifstream &file, SaveState &state) { \
			read(file, state.arg.ptr, state.arg.size()); \
		} \
	}; \
	pushSaver(list, label, Func::save, Func::load, sizeof label); \
} while (0)

#define ADDARRAY(arg) do { \
	struct Func { \
		static void save(ofstream &file, SaveState const &state) { \
			write(file, state.arg, sizeof state.arg); \
		} \
		static void load(ifstream &file, SaveState &state) { \
			read(file, state.arg, sizeof state.arg); \
		} \
	}; \
	pushSaver(list, label, Func::save, Func::load, sizeof label); \
} while (0)

	{ static char const label[] = { c,c,           NUL }; ADD(cpu.cycleCounter); }
	{ static char const label[] = { p,c,           NUL }; ADD(cpu.pc); }
	{ static char const label[] = { s,p,           NUL }; ADD(cpu.sp); }
	{ static char const label[] = { a,             NUL }; ADD(cpu.a); }
	{ static char const label[] = { b,             NUL }; ADD(cpu.b); }
	{ static char const label[] = { c,             NUL }; ADD(cpu.c); }
	{ static char const label[] = { d,             NUL }; ADD(cpu.d); }
	{ static char const label[] = { e,             NUL }; ADD(cpu.e); }
	{ static char const label[] = { f,             NUL }; ADD(cpu.f); }
	{ static char const label[] = { h,             NUL }; ADD(cpu.h); }
	{ static char const label[] = { l,             NUL }; ADD(cpu.l); }
	{ static char const label[] = { s,k,i,p,       NUL }; ADD(cpu.skip); }
	{ static char const label[] = { h,a,n,g,       NUL }; ADD(cpu.hang); }
	{ static char const label[] = { h,a,l,t,       NUL }; ADD(mem.halted); }
	{ static char const label[] = { v,r,a,m,       NUL }; ADDPTR(mem.vram); }
	{ static char const label[] = { s,r,a,m,       NUL }; ADDPTR(mem.sram); }
	{ static char const label[] = { w,r,a,m,       NUL }; ADDPTR(mem.wram); }
	{ static char const label[] = { h,r,a,m,       NUL }; ADDPTR(mem.ioamhram); }
	{ static char const label[] = { l,d,i,v,u,p,   NUL }; ADD(mem.divLastUpdate); }
	{ static char const label[] = { l,t,i,m,a,u,p, NUL }; ADD(mem.timaLastUpdate); }
	{ static char const label[] = { t,m,a,t,i,m,e, NUL }; ADD(mem.tmatime); }
	{ static char const label[] = { s,e,r,i,a,l,t, NUL }; ADD(mem.nextSerialtime); }
	{ static char const label[] = { l,o,d,m,a,u,p, NUL }; ADD(mem.lastOamDmaUpdate); }
	{ static char const label[] = { m,i,n,i,n,t,t, NUL }; ADD(mem.minIntTime); }
	{ static char const label[] = { u,n,h,a,l,t,t, NUL }; ADD(mem.unhaltTime); }
	{ static char const label[] = { r,o,m,b,a,n,k, NUL }; ADD(mem.rombank); }
	{ static char const label[] = { d,m,a,s,r,c,   NUL }; ADD(mem.dmaSource); }
	{ static char const label[] = { d,m,a,d,s,t,   NUL }; ADD(mem.dmaDestination); }
	{ static char const label[] = { r,a,m,b,a,n,k, NUL }; ADD(mem.rambank); }
	{ static char const label[] = { o,d,m,a,p,o,s, NUL }; ADD(mem.oamDmaPos); }
	{ static char const label[] = { i,m,e,         NUL }; ADD(mem.IME); }
	{ static char const label[] = { s,r,a,m,o,n,   NUL }; ADD(mem.enableRam); }
	{ static char const label[] = { r,a,m,b,m,o,d, NUL }; ADD(mem.rambankMode); }
	{ static char const label[] = { h,d,m,a,       NUL }; ADD(mem.hdmaTransfer); }
	{ static char const label[] = { b,g,p,         NUL }; ADDPTR(ppu.bgpData); }
	{ static char const label[] = { o,b,j,p,       NUL }; ADDPTR(ppu.objpData); }
	{ static char const label[] = { s,p,o,s,b,u,f, NUL }; ADDPTR(ppu.oamReaderBuf); }
	{ static char const label[] = { s,p,s,z,b,u,f, NUL }; ADDPTR(ppu.oamReaderSzbuf); }
	{ static char const label[] = { s,p,a,t,t,r,   NUL }; ADDARRAY(ppu.spAttribList); }
	{ static char const label[] = { s,p,b,y,t,e,NO0, NUL }; ADDARRAY(ppu.spByte0List); }
	{ static char const label[] = { s,p,b,y,t,e,NO1, NUL }; ADDARRAY(ppu.spByte1List); }
	{ static char const label[] = { v,c,y,c,l,e,s, NUL }; ADD(ppu.videoCycles); }
	{ static char const label[] = { e,d,M,NO0,t,i,m, NUL }; ADD(ppu.enableDisplayM0Time); }
	{ static char const label[] = { m,NO0,t,i,m,e, NUL }; ADD(ppu.lastM0Time); }
	{ static char const label[] = { n,m,NO0,i,r,q, NUL }; ADD(ppu.nextM0Irq); }
	{ static char const label[] = { b,g,t,w,       NUL }; ADD(ppu.tileword); }
	{ static char const label[] = { b,g,n,t,w,     NUL }; ADD(ppu.ntileword); }
	{ static char const label[] = { w,i,n,y,p,o,s, NUL }; ADD(ppu.winYPos); }
	{ static char const label[] = { x,p,o,s,       NUL }; ADD(ppu.xpos); }
	{ static char const label[] = { e,n,d,x,       NUL }; ADD(ppu.endx); }
	{ static char const label[] = { p,p,u,r,NO0,   NUL }; ADD(ppu.reg0); }
	{ static char const label[] = { p,p,u,r,NO1,   NUL }; ADD(ppu.reg1); }
	{ static char const label[] = { b,g,a,t,r,b,   NUL }; ADD(ppu.attrib); }
	{ static char const label[] = { b,g,n,a,t,r,b, NUL }; ADD(ppu.nattrib); }
	{ static char const label[] = { p,p,u,s,t,a,t, NUL }; ADD(ppu.state); }
	{ static char const label[] = { n,s,p,r,i,t,e, NUL }; ADD(ppu.nextSprite); }
	{ static char const label[] = { c,s,p,r,i,t,e, NUL }; ADD(ppu.currentSprite); }
	{ static char const label[] = { l,y,c,         NUL }; ADD(ppu.lyc); }
	{ static char const label[] = { m,NO0,l,y,c,   NUL }; ADD(ppu.m0lyc); }
	{ static char const label[] = { o,l,d,w,y,     NUL }; ADD(ppu.oldWy); }
	{ static char const label[] = { w,i,n,d,r,a,w, NUL }; ADD(ppu.winDrawState); }
	{ static char const label[] = { w,s,c,x,       NUL }; ADD(ppu.wscx); }
	{ static char const label[] = { w,e,m,a,s,t,r, NUL }; ADD(ppu.weMaster); }
	{ static char const label[] = { l,c,d,s,i,r,q, NUL }; ADD(ppu.pendingLcdstatIrq); }
	{ static char const label[] = { s,p,u,c,n,t,r, NUL }; ADD(spu.cycleCounter); }
	{ static char const label[] = { s,w,p,c,n,t,r, NUL }; ADD(spu.ch1.sweep.counter); }
	{ static char const label[] = { s,w,p,s,h,d,w, NUL }; ADD(spu.ch1.sweep.shadow); }
	{ static char const label[] = { s,w,p,n,e,g,   NUL }; ADD(spu.ch1.sweep.negging); }
	{ static char const label[] = { d,u,t,NO1,c,t,r, NUL }; ADD(spu.ch1.duty.nextPosUpdate); }
	{ static char const label[] = { d,u,t,NO1,p,o,s, NUL }; ADD(spu.ch1.duty.pos); }
	{ static char const label[] = { d,u,t,NO1,h,i,   NUL }; ADD(spu.ch1.duty.high); }
	{ static char const label[] = { e,n,v,NO1,c,t,r, NUL }; ADD(spu.ch1.env.counter); }
	{ static char const label[] = { e,n,v,NO1,v,o,l, NUL }; ADD(spu.ch1.env.volume); }
	{ static char const label[] = { l,e,n,NO1,c,t,r, NUL }; ADD(spu.ch1.lcounter.counter); }
	{ static char const label[] = { l,e,n,NO1,v,a,l, NUL }; ADD(spu.ch1.lcounter.lengthCounter); }
	{ static char const label[] = { n,r,NO1,NO0,       NUL }; ADD(spu.ch1.sweep.nr0); }
	{ static char const label[] = { n,r,NO1,NO3,       NUL }; ADD(spu.ch1.duty.nr3); }
	{ static char const label[] = { n,r,NO1,NO4,       NUL }; ADD(spu.ch1.nr4); }
	{ static char const label[] = { c,NO1,m,a,s,t,r, NUL }; ADD(spu.ch1.master); }
	{ static char const label[] = { d,u,t,NO2,c,t,r, NUL }; ADD(spu.ch2.duty.nextPosUpdate); }
	{ static char const label[] = { d,u,t,NO2,p,o,s, NUL }; ADD(spu.ch2.duty.pos); }
	{ static char const label[] = { d,u,t,NO2,h,i,   NUL }; ADD(spu.ch2.duty.high); }
	{ static char const label[] = { e,n,v,NO2,c,t,r, NUL }; ADD(spu.ch2.env.counter); }
	{ static char const label[] = { e,n,v,NO2,v,o,l, NUL }; ADD(spu.ch2.env.volume); }
	{ static char const label[] = { l,e,n,NO2,c,t,r, NUL }; ADD(spu.ch2.lcounter.counter); }
	{ static char const label[] = { l,e,n,NO2,v,a,l, NUL }; ADD(spu.ch2.lcounter.lengthCounter); }
	{ static char const label[] = { n,r,NO2,NO3,       NUL }; ADD(spu.ch2.duty.nr3); }
	{ static char const label[] = { n,r,NO2,NO4,       NUL }; ADD(spu.ch2.nr4); }
	{ static char const label[] = { c,NO2,m,a,s,t,r, NUL }; ADD(spu.ch2.master); }
	{ static char const label[] = { w,a,v,e,r,a,m, NUL }; ADDPTR(spu.ch3.waveRam); }
	{ static char const label[] = { l,e,n,NO3,c,t,r, NUL }; ADD(spu.ch3.lcounter.counter); }
	{ static char const label[] = { l,e,n,NO3,v,a,l, NUL }; ADD(spu.ch3.lcounter.lengthCounter); }
	{ static char const label[] = { w,a,v,e,c,t,r, NUL }; ADD(spu.ch3.waveCounter); }
	{ static char const label[] = { l,w,a,v,r,d,t, NUL }; ADD(spu.ch3.lastReadTime); }
	{ static char const label[] = { w,a,v,e,p,o,s, NUL }; ADD(spu.ch3.wavePos); }
	{ static char const label[] = { w,a,v,s,m,p,l, NUL }; ADD(spu.ch3.sampleBuf); }
	{ static char const label[] = { n,r,NO3,NO3,       NUL }; ADD(spu.ch3.nr3); }
	{ static char const label[] = { n,r,NO3,NO4,       NUL }; ADD(spu.ch3.nr4); }
	{ static char const label[] = { c,NO3,m,a,s,t,r, NUL }; ADD(spu.ch3.master); }
	{ static char const label[] = { l,f,s,r,c,t,r, NUL }; ADD(spu.ch4.lfsr.counter); }
	{ static char const label[] = { l,f,s,r,r,e,g, NUL }; ADD(spu.ch4.lfsr.reg); }
	{ static char const label[] = { e,n,v,NO4,c,t,r, NUL }; ADD(spu.ch4.env.counter); }
	{ static char const label[] = { e,n,v,NO4,v,o,l, NUL }; ADD(spu.ch4.env.volume); }
	{ static char const label[] = { l,e,n,NO4,c,t,r, NUL }; ADD(spu.ch4.lcounter.counter); }
	{ static char const label[] = { l,e,n,NO4,v,a,l, NUL }; ADD(spu.ch4.lcounter.lengthCounter); }
	{ static char const label[] = { n,r,NO4,NO4,       NUL }; ADD(spu.ch4.nr4); }
	{ static char const label[] = { c,NO4,m,a,s,t,r, NUL }; ADD(spu.ch4.master); }
	{ static char const label[] = { r,t,c,b,a,s,e, NUL }; ADD(rtc.baseTime); }
	{ static char const label[] = { r,t,c,h,a,l,t, NUL }; ADD(rtc.haltTime); }
	{ static char const label[] = { r,t,c,d,h,     NUL }; ADD(rtc.dataDh); }
	{ static char const label[] = { r,t,c,d,l,     NUL }; ADD(rtc.dataDl); }
	{ static char const label[] = { r,t,c,h,       NUL }; ADD(rtc.dataH); }
	{ static char const label[] = { r,t,c,m,       NUL }; ADD(rtc.dataM); }
	{ static char const label[] = { r,t,c,s,       NUL }; ADD(rtc.dataS); }
	{ static char const label[] = { r,t,c,l,l,d,   NUL }; ADD(rtc.lastLatchData); }

#undef ADD
#undef ADDPTR
#undef ADDARRAY

	list.resize(list.size());
	sort(list.begin(), list.end());

	maxLabelsize_ = 0;

	for (size_t i = 0; i < list.size(); ++i) {
		if (list[i].labelsize > maxLabelsize_)
			maxLabelsize_ = list[i].labelsize;
	}
}

}

namespace {

static void writeSnapShot(ofstream &file, uint_least32_t const *pixels, ptrdiff_t const pitch) {
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
		ptrdiff_t const pitch, string const &filename) {
	ofstream file(filename.c_str(), ios_base::binary);
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

bool StateSaver::loadState(SaveState &state, string const &filename) {
	ifstream file(filename.c_str(), ios_base::binary);
	if (!file || file.get() != 0)
		return false;

	file.ignore();
	file.ignore(get24(file));

	Array<char> const labelbuf(list.maxLabelsize());
	Saver const labelbufSaver = { labelbuf, 0, 0, list.maxLabelsize() };
	SaverList::const_iterator done = list.begin();

	while (file.good() && done != list.end()) {
		file.getline(labelbuf, list.maxLabelsize(), NUL);

		SaverList::const_iterator it = done;
		if (strcmp(labelbuf, it->label)) {
			it = lower_bound(it + 1, list.end(), labelbufSaver);

			if (it == list.end() || strcmp(labelbuf, it->label)) {
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
