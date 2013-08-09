/***************************************************************************
 *   Copyright (C) 2007 by Sindre Aamås                                    *
 *   sinamas@users.sourceforge.net                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License version 2 for more details.                *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   version 2 along with this program; if not, write to the               *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef SOUND_CHANNEL4_H
#define SOUND_CHANNEL4_H

#include "envelope_unit.h"
#include "gbint.h"
#include "length_counter.h"
#include "master_disabler.h"
#include "static_output_tester.h"
#include "loadsave.h"

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

namespace gambatte {

struct SaveState;

class Channel4 {
public:
	Channel4();
	void setNr1(unsigned data);
	void setNr2(unsigned data);
	void setNr3(unsigned data) { lfsr_.nr3Change(data, cycleCounter_); }
	void setNr4(unsigned data);
	void loadOrSave(loadsave& state);
	void setSo(unsigned soMask);
	bool isActive() const { return master_; }
	void update(uint_least32_t *buf, unsigned soBaseVol, unsigned cycles);
	void reset();
	void init(bool cgb);
	void saveState(SaveState &state);
	void loadState(SaveState const &state);

private:
	class Lfsr : public SoundUnit {
	public:
		Lfsr();
		virtual void event();
		void loadOrSave(loadsave& state) {
			loadOrSave2(state);
			state(backupCounter_);
			state(reg_);
			state(nr3_);
			state(master_);
		}
		virtual void resetCounters(unsigned oldCc);
		bool isHighState() const { return ~reg_ & 1; }
		void nr3Change(unsigned newNr3, unsigned cc);
		void nr4Init(unsigned cc);
		void reset(unsigned cc);
		void saveState(SaveState &state, unsigned cc);
		void loadState(SaveState const &state);
		void disableMaster() { killCounter(); master_ = false; reg_ = 0x7FFF; }
		void killCounter() { counter_ = counter_disabled; }
		void reviveCounter(unsigned cc);

	private:
		unsigned backupCounter_;
		unsigned short reg_;
		unsigned char nr3_;
		bool master_;

		void updateBackupCounter(unsigned cc);
	};

	class Ch4MasterDisabler : public MasterDisabler {
	public:
		Ch4MasterDisabler(bool &m, Lfsr &lfsr) : MasterDisabler(m), lfsr_(lfsr) {}
		virtual void operator()() { MasterDisabler::operator()(); lfsr_.disableMaster(); }

	private:
		Lfsr &lfsr_;
	};

	friend class StaticOutputTester<Channel4, Lfsr>;

	StaticOutputTester<Channel4, Lfsr> staticOutputTest_;
	Ch4MasterDisabler disableMaster_;
	LengthCounter lengthCounter_;
	EnvelopeUnit envelopeUnit_;
	Lfsr lfsr_;
	SoundUnit *nextEventUnit_;
	unsigned cycleCounter_;
	unsigned soMask_;
	unsigned prevOut_;
	unsigned char nr4_;
	bool master_;

	void setEvent();
};

}

#endif
