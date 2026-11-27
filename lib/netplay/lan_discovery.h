/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** \file
 *  Local-network ("Local Player") room discovery.
 *
 *  A hosting device broadcasts a small UDP "beacon" about once a second on every
 *  active network interface (Wi-Fi, phone hotspot, ...). Devices that are browsing
 *  listen for those beacons and show them as joinable rooms. The game itself is
 *  still joined over the normal TCP connection to the address the beacon came from.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct LanRoomInfo
{
	std::string address;       // IPv4 address of the host, as seen on the network
	uint16_t port = 0;         // TCP game port of the host
	std::string hostName;      // host player's name
	std::string mapName;
	int players = 0;
	int maxPlayers = 0;
	bool compatible = true;    // same netcode version as this device
};

namespace LanDiscovery
{
	// --- hosting ("Create Room") ---

	/// Call regularly (every frame is fine) while a room is open. Sends a beacon at most once a second.
	void hostUpdate(const std::string &hostName, const std::string &mapName, int players, int maxPlayers, uint16_t port);
	/// Stop announcing the room and release the socket.
	void hostStop();

	// --- browsing ("Join Room") ---

	void browseStart();
	void browseStop();
	/// Receives pending beacons and returns the rooms currently visible (rooms that stop beaconing expire after a few seconds).
	std::vector<LanRoomInfo> browsePoll();

	// --- information ---

	/// This device's IPv4 addresses on active non-loopback interfaces (useful to show "your address").
	std::vector<std::string> localAddresses();
}
