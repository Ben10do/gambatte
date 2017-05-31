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

#ifndef GAMBATTE_SERIALGETTER_H
#define GAMBATTE_SERIALGETTER_H

namespace gambatte {

class SerialGetter {
public:
	/**
	 * Receive a byte from a linked Game Boy.
	 * @param isMaster True if this Game Boy is the master; false if it's the slave
	 * @return The byte that this Game Boy should receive.
	 */
	virtual unsigned char get(bool isMaster) = 0;

	/**
	 * Send a byte to a linked Game Boy.
	 * @param data The byte to be sent by this Game Boy.
	 * @param isMaster True if this Game Boy is the master; false if it's the slave
	 */
	virtual void send(unsigned char data, bool isMaster) = 0;

	/**
	 * Check if any data has been recieved from the other Game Boy.
	 * @return If data has been recieved from another Game Boy
	 */
	virtual bool isDataAvailable() = 0;

	/**
	 * Set the transfer speed for when this Game Boy is the master.
	 * @param doubleSpeed True if the Game Boy is running in GBC's double speed mode
	 * @param fastTransfer True if bit 1 is set in SC (0xFF02)
	 */
	virtual void setSpeed(bool doubleSpeed, bool fastTransfer) = 0;

	/**
	 * Get the transfer speed of the other Game Boy, when it is the master.
	 * @param doubleSpeed True if the Game Boy is running in GBC's double speed mode
	 * @param fastTransfer True if bit 1 is set in SC (0xFF02)
	 */
	virtual void getSpeed(bool &doubleSpeed, bool &fastTransfer) = 0;

	virtual ~SerialGetter() {}
};

}

#endif
