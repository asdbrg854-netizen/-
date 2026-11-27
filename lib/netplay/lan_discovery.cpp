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

#include "lib/framework/frame.h"
#include "lan_discovery.h"
#include "lib/netplay/netplay.h"

#if defined(__ANDROID__) || defined(__linux__) || defined(__APPLE__)
# define WZ_LAN_DISCOVERY_POSIX 1
#endif

#if defined(WZ_LAN_DISCOVERY_POSIX)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace
{

constexpr uint16_t LAN_DISCOVERY_PORT = 2122; // UDP; the game itself uses TCP (default 2100)
constexpr const char *LAN_MAGIC = "WZLAN1";
constexpr size_t LAN_MAX_NAME_BYTES = 24;
const std::chrono::milliseconds BEACON_INTERVAL(1000);
const std::chrono::milliseconds ROOM_TIMEOUT(4000);

int sendSocket = -1;
int recvSocket = -1;
bool beaconSentOnce = false;
std::chrono::steady_clock::time_point lastBeacon;

struct SeenRoom
{
	LanRoomInfo info;
	std::chrono::steady_clock::time_point lastSeen;
};
std::map<std::string, SeenRoom> seenRooms;

void closeSocket(int &fd)
{
	if (fd >= 0)
	{
		close(fd);
		fd = -1;
	}
}

void setNonBlocking(int fd)
{
	const int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
	{
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
}

int openSendSocket()
{
	const int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
	{
		debug(LOG_NET, "LAN discovery: cannot create send socket: %s", strerror(errno));
		return -1;
	}
	const int yes = 1;
	setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
	setNonBlocking(fd);
	return fd;
}

int openRecvSocket()
{
	const int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
	{
		debug(LOG_NET, "LAN discovery: cannot create receive socket: %s", strerror(errno));
		return -1;
	}
	const int yes = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#if defined(SO_REUSEPORT)
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
	setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(LAN_DISCOVERY_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
	{
		debug(LOG_NET, "LAN discovery: cannot bind UDP port %u: %s", static_cast<unsigned>(LAN_DISCOVERY_PORT), strerror(errno));
		close(fd);
		return -1;
	}
	setNonBlocking(fd);
	return fd;
}

// Keep names short, single-line and free of the field separator; never cut a UTF-8 character in half.
std::string sanitizeName(const std::string &input)
{
	std::string out;
	out.reserve(input.size());
	for (const char c : input)
	{
		const unsigned char uc = static_cast<unsigned char>(c);
		out.push_back((c == '|' || uc < 0x20) ? ' ' : c);
	}
	if (out.size() > LAN_MAX_NAME_BYTES)
	{
		out.resize(LAN_MAX_NAME_BYTES);
		// drop an incomplete trailing multi-byte sequence
		while (!out.empty())
		{
			const unsigned char last = static_cast<unsigned char>(out.back());
			if ((last & 0xC0) == 0x80)
			{
				out.pop_back(); // continuation byte, keep looking for its lead byte
			}
			else if (last >= 0xC0)
			{
				out.pop_back(); // lead byte whose continuation bytes were cut off
				break;
			}
			else
			{
				break; // plain ASCII, the string is complete
			}
		}
	}
	return out;
}

// All addresses a beacon should be sent to: the global broadcast address plus the directed broadcast
// address of every active interface (needed when this device is the hotspot, because the global
// broadcast only leaves through the default-route interface, which is then the mobile data link).
std::vector<sockaddr_in> broadcastTargets()
{
	std::vector<sockaddr_in> targets;

	sockaddr_in global;
	memset(&global, 0, sizeof(global));
	global.sin_family = AF_INET;
	global.sin_port = htons(LAN_DISCOVERY_PORT);
	global.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	targets.push_back(global);

	struct ifaddrs *interfaces = nullptr;
	if (getifaddrs(&interfaces) == 0)
	{
		for (const struct ifaddrs *ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_addr == nullptr || ifa->ifa_netmask == nullptr || ifa->ifa_addr->sa_family != AF_INET)
			{
				continue;
			}
			if (!(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_BROADCAST))
			{
				continue;
			}
			const sockaddr_in *addr = reinterpret_cast<const sockaddr_in *>(ifa->ifa_addr);
			const sockaddr_in *mask = reinterpret_cast<const sockaddr_in *>(ifa->ifa_netmask);

			sockaddr_in directed;
			memset(&directed, 0, sizeof(directed));
			directed.sin_family = AF_INET;
			directed.sin_port = htons(LAN_DISCOVERY_PORT);
			directed.sin_addr.s_addr = (addr->sin_addr.s_addr & mask->sin_addr.s_addr) | ~mask->sin_addr.s_addr;
			targets.push_back(directed);
		}
		freeifaddrs(interfaces);
	}
	if (targets.size() == 1)
	{
		// Interface enumeration returned nothing (restricted on some Android versions). Also try the usual
		// phone-hotspot subnet, so a hosting hotspot device can still be found by its clients.
		sockaddr_in hotspot;
		memset(&hotspot, 0, sizeof(hotspot));
		hotspot.sin_family = AF_INET;
		hotspot.sin_port = htons(LAN_DISCOVERY_PORT);
		inet_pton(AF_INET, "192.168.43.255", &hotspot.sin_addr);
		targets.push_back(hotspot);
	}
	return targets;
}

std::vector<std::string> splitFields(const std::string &text, char separator)
{
	std::vector<std::string> fields;
	size_t start = 0;
	for (;;)
	{
		const size_t pos = text.find(separator, start);
		if (pos == std::string::npos)
		{
			fields.push_back(text.substr(start));
			break;
		}
		fields.push_back(text.substr(start, pos - start));
		start = pos + 1;
	}
	return fields;
}

bool parseBeacon(const std::string &packet, const char *senderAddress, LanRoomInfo &out)
{
	// WZLAN1|netcodeMajor|netcodeMinor|port|players|maxPlayers|hostName|mapName
	const std::vector<std::string> f = splitFields(packet, '|');
	if (f.size() < 8 || f[0] != LAN_MAGIC)
	{
		return false;
	}
	const long major = strtol(f[1].c_str(), nullptr, 10);
	const long minor = strtol(f[2].c_str(), nullptr, 10);
	const long port = strtol(f[3].c_str(), nullptr, 10);
	if (port <= 0 || port > 65535)
	{
		return false;
	}
	out.address = senderAddress;
	out.port = static_cast<uint16_t>(port);
	out.players = static_cast<int>(strtol(f[4].c_str(), nullptr, 10));
	out.maxPlayers = static_cast<int>(strtol(f[5].c_str(), nullptr, 10));
	out.hostName = f[6];
	out.mapName = f[7];
	out.compatible = (major == static_cast<long>(NETGetMajorVersion()) && minor == static_cast<long>(NETGetMinorVersion()));
	return true;
}

} // anonymous namespace

namespace LanDiscovery
{

void hostUpdate(const std::string &hostName, const std::string &mapName, int players, int maxPlayers, uint16_t port)
{
	const auto now = std::chrono::steady_clock::now();
	if (beaconSentOnce && now - lastBeacon < BEACON_INTERVAL)
	{
		return;
	}
	lastBeacon = now;
	beaconSentOnce = true;

	if (sendSocket < 0)
	{
		sendSocket = openSendSocket();
		if (sendSocket < 0)
		{
			return;
		}
	}

	char packet[256];
	const int length = snprintf(packet, sizeof(packet), "%s|%u|%u|%u|%d|%d|%s|%s",
		LAN_MAGIC,
		static_cast<unsigned>(NETGetMajorVersion()), static_cast<unsigned>(NETGetMinorVersion()),
		static_cast<unsigned>(port), players, maxPlayers,
		sanitizeName(hostName).c_str(), sanitizeName(mapName).c_str());
	if (length <= 0 || static_cast<size_t>(length) >= sizeof(packet))
	{
		return;
	}

	for (const sockaddr_in &target : broadcastTargets())
	{
		// Errors are expected for interfaces that cannot broadcast (e.g. mobile data); just try the next one.
		sendto(sendSocket, packet, static_cast<size_t>(length), 0, reinterpret_cast<const sockaddr *>(&target), sizeof(target));
	}
}

void hostStop()
{
	closeSocket(sendSocket);
	beaconSentOnce = false;
}

void browseStart()
{
	if (recvSocket < 0)
	{
		recvSocket = openRecvSocket();
	}
}

void browseStop()
{
	closeSocket(recvSocket);
	seenRooms.clear();
}

std::vector<LanRoomInfo> browsePoll()
{
	const auto now = std::chrono::steady_clock::now();

	if (recvSocket >= 0)
	{
		char buffer[512];
		for (int i = 0; i < 64; ++i) // bounded work per call
		{
			sockaddr_in from;
			socklen_t fromLength = sizeof(from);
			const ssize_t received = recvfrom(recvSocket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr *>(&from), &fromLength);
			if (received <= 0)
			{
				break; // nothing (more) to read right now
			}
			buffer[received] = '\0';

			char senderAddress[INET_ADDRSTRLEN] = {0};
			if (inet_ntop(AF_INET, &from.sin_addr, senderAddress, sizeof(senderAddress)) == nullptr)
			{
				continue;
			}
			LanRoomInfo info;
			if (!parseBeacon(buffer, senderAddress, info))
			{
				continue;
			}
			const std::string key = info.address + ":" + std::to_string(info.port);
			SeenRoom &seen = seenRooms[key];
			seen.info = info;
			seen.lastSeen = now;
		}
	}

	std::vector<LanRoomInfo> rooms;
	for (auto it = seenRooms.begin(); it != seenRooms.end();)
	{
		if (now - it->second.lastSeen > ROOM_TIMEOUT)
		{
			it = seenRooms.erase(it);
			continue;
		}
		rooms.push_back(it->second.info);
		++it;
	}
	std::sort(rooms.begin(), rooms.end(), [](const LanRoomInfo &a, const LanRoomInfo &b) {
		if (a.hostName != b.hostName) { return a.hostName < b.hostName; }
		return a.address < b.address;
	});
	return rooms;
}

std::vector<std::string> localAddresses()
{
	std::vector<std::string> addresses;
	struct ifaddrs *interfaces = nullptr;
	if (getifaddrs(&interfaces) != 0)
	{
		return addresses;
	}
	for (const struct ifaddrs *ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
		{
			continue;
		}
		if (!(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK))
		{
			continue;
		}
		char text[INET_ADDRSTRLEN] = {0};
		const sockaddr_in *addr = reinterpret_cast<const sockaddr_in *>(ifa->ifa_addr);
		if (inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text)) != nullptr)
		{
			addresses.emplace_back(text);
		}
	}
	freeifaddrs(interfaces);
	return addresses;
}

} // namespace LanDiscovery

#else // !WZ_LAN_DISCOVERY_POSIX

// Local-network discovery is only implemented for POSIX platforms (Android, Linux, macOS).
namespace LanDiscovery
{
void hostUpdate(const std::string &, const std::string &, int, int, uint16_t) {}
void hostStop() {}
void browseStart() {}
void browseStop() {}
std::vector<LanRoomInfo> browsePoll() { return {}; }
std::vector<std::string> localAddresses() { return {}; }
}

#endif
