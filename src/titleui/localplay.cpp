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
 *  Local Player title UI (Create Room / Join Room over the local network).
 */

#include "lib/framework/frame.h"
#include "localplay.h"
#include "multiplayer.h"
#include "lib/ivis_opengl/screen.h"
#include "lib/netplay/netplay.h"
#include "lib/widget/widget.h"
#include "lib/widget/button.h"
#include "../frontend.h"
#include "../frend.h"
#include "../hci.h"
#include "../intdisplay.h"
#include "../levels.h"
#include "../multiint.h"
#include "../multijoin_helpers.h"
#include "../multiplay.h"
#include "../warzoneconfig.h"

#include <algorithm>
#include <chrono>

namespace
{

// Widget ids (kept well away from the other frontend ids)
enum LocalPlayWidgetId : unsigned
{
	LOCALPLAY_CREATE = 4598100,
	LOCALPLAY_JOIN,
	LOCALPLAY_BACK,
	LOCALPLAY_HINT,
	LOCALPLAY_STATUS,
	LOCALPLAY_REFRESH,
	LOCALPLAY_HELP,
	LOCALPLAY_ROOM0 = 4598200 // + room index
};

constexpr size_t MAX_ROOM_BUTTONS = 6;

// Easier connecting: when exactly one joinable room is visible and stays visible for this long,
// join it automatically (no extra tap). Set to false to always require a tap.
constexpr bool AUTO_JOIN_SINGLE_ROOM = true;
const std::chrono::milliseconds AUTO_JOIN_DELAY(1200);
// If nothing is found for this long, show troubleshooting help.
const std::chrono::milliseconds HELP_DELAY(6000);

std::chrono::steady_clock::time_point joinPageStart;
std::chrono::steady_clock::time_point singleRoomSince;
std::string singleRoomKey;

bool bBroadcastRoom = false;   // announce the hosted room on the local network
bool bAutoHostRequest = false; // lobby should start hosting right away

std::string truncateForButton(const std::string &text, size_t maxBytes)
{
	if (text.size() <= maxBytes)
	{
		return text;
	}
	std::string out = text.substr(0, maxBytes);
	while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80)
	{
		out.pop_back(); // don't cut a UTF-8 character in half
	}
	if (!out.empty() && static_cast<unsigned char>(out.back()) >= 0xC0)
	{
		out.pop_back();
	}
	return out + "...";
}

std::string roomLabel(const LanRoomInfo &room)
{
	if (!room.compatible)
	{
		return astringf(_("%s - different game version"), truncateForButton(room.hostName, 18).c_str());
	}
	return astringf("%s  (%d/%d)  %s", truncateForButton(room.hostName, 18).c_str(), room.players, room.maxPlayers, truncateForButton(room.mapName, 18).c_str());
}

std::string roomsSignatureOf(const std::vector<LanRoomInfo> &rooms)
{
	std::string signature;
	for (const LanRoomInfo &room : rooms)
	{
		signature += room.address + ":" + std::to_string(room.port) + "|" + room.hostName + "|" + room.mapName + "|"
			+ std::to_string(room.players) + "/" + std::to_string(room.maxPlayers) + "|" + (room.compatible ? "1" : "0") + ";";
	}
	return signature;
}

} // anonymous namespace

// MARK: - Hooks used by the lobby UI

bool localPlayConsumeAutoHostRequest()
{
	const bool request = bAutoHostRequest;
	bAutoHostRequest = false;
	return request;
}

void localPlayHostTick()
{
	if (!bBroadcastRoom || !NetPlay.bComms || !NetPlay.isHost)
	{
		return; // not hosting a Local Player room (yet)
	}
	LanDiscovery::hostUpdate(sPlayer, game.map, static_cast<int>(NetPlay.playercount), static_cast<int>(game.maxPlayers), static_cast<uint16_t>(NETgetGameserverPort()));
}

void localPlayHostStop()
{
	bBroadcastRoom = false;
	bAutoHostRequest = false;
	LanDiscovery::hostStop();
}

void localPlayReset()
{
	localPlayHostStop();
	NETsetLanOnlyHosting(false);
}

// MARK: - WzLocalPlayTitleUI

WzLocalPlayTitleUI::WzLocalPlayTitleUI(std::shared_ptr<WzTitleUI> parent)
	: parent(parent)
{
}

WzLocalPlayTitleUI::~WzLocalPlayTitleUI()
{
	LanDiscovery::browseStop();
}

void WzLocalPlayTitleUI::start()
{
	// Called when this UI is shown, including when coming back from the lobby: always start at the main page
	LanDiscovery::browseStop();
	page = Page::Menu;
	joining = false;
	showMenuPage();
}

void WzLocalPlayTitleUI::showMenuPage()
{
	addBackdrop();
	addTopForm(false);
	addBottomForm();

	addTextButton(LOCALPLAY_CREATE, FRONTEND_POS2X, FRONTEND_POS2Y, _("Create Room"), WBUT_TXTCENTRE);
	addTextButton(LOCALPLAY_JOIN, FRONTEND_POS3X, FRONTEND_POS3Y, _("Join Room"), WBUT_TXTCENTRE);
	addSideText(FRONTEND_SIDETEXT, FRONTEND_SIDEX, FRONTEND_SIDEY, _("LOCAL PLAYER"));
	addMultiBut(psWScreen, FRONTEND_BOTFORM, LOCALPLAY_BACK, 10, 10, 30, 29, P_("menu", "Return"), IMAGE_RETURN, IMAGE_RETURN_HI, IMAGE_RETURN_HI);

	// Hint: how to connect, and this device's address (useful if a friend wants to join by IP)
	std::string hint = _("Connect all devices to the same Wi-Fi, or turn on a phone hotspot.");
	const std::vector<std::string> addresses = LanDiscovery::localAddresses();
	if (!addresses.empty())
	{
		hint += "  ";
		hint += astringf(_("Your address: %s"), addresses.front().c_str());
	}
	addSmallTextButton(LOCALPLAY_HINT, FRONTEND_POS9X, FRONTEND_POS9Y, hint.c_str(), 0);
}

void WzLocalPlayTitleUI::showJoinPage()
{
	page = Page::JoinList;
	joining = false;
	rooms.clear();
	roomsSignature.clear();
	LanDiscovery::browseStart();
	joinPageStart = std::chrono::steady_clock::now();
	singleRoomKey.clear();

	widgDelete(psWScreen, FRONTEND_BACKDROP); // remove the menu page
	addBackdrop();
	addTopForm(false);
	addBottomForm();

	addSideText(FRONTEND_SIDETEXT, FRONTEND_SIDEX, FRONTEND_SIDEY, _("JOIN ROOM"));
	addMultiBut(psWScreen, FRONTEND_BOTFORM, LOCALPLAY_BACK, 10, 10, 30, 29, P_("menu", "Return"), IMAGE_RETURN, IMAGE_RETURN_HI, IMAGE_RETURN_HI);
	addTextButton(LOCALPLAY_REFRESH, FRONTEND_POS8X, FRONTEND_POS8Y, _("Refresh"), WBUT_TXTCENTRE);

	rebuildRoomButtons();
}

void WzLocalPlayTitleUI::rebuildRoomButtons()
{
	widgDelete(psWScreen, LOCALPLAY_STATUS);
	for (size_t i = 0; i < MAX_ROOM_BUTTONS; ++i)
	{
		widgDelete(psWScreen, LOCALPLAY_ROOM0 + static_cast<unsigned>(i));
	}

	if (rooms.empty())
	{
		addTextButton(LOCALPLAY_STATUS, FRONTEND_POS2X, FRONTEND_POS2Y, _("Searching for rooms..."), WBUT_TXTCENTRE | WBUT_DISABLE);
		return;
	}

	const size_t count = std::min(rooms.size(), MAX_ROOM_BUTTONS);
	for (size_t i = 0; i < count; ++i)
	{
		const unsigned style = WBUT_TXTCENTRE | (rooms[i].compatible ? 0u : static_cast<unsigned>(WBUT_DISABLE));
		addTextButton(LOCALPLAY_ROOM0 + static_cast<unsigned>(i), FRONTEND_POS2X, FRONTEND_POS2Y + static_cast<UDWORD>(i) * FRONTEND_BUTHEIGHT, roomLabel(rooms[i]), style);
	}
}

void WzLocalPlayTitleUI::createRoom()
{
	LanDiscovery::browseStop();

	// First of all, make sure we've reset any prior networking state (same steps as "Host Game")
	NETshutdown();
	NetPlay.bComms = true;
	ingame.side = InGameSide::HOST_OR_SINGLEPLAYER;
	bMultiPlayer = true;
	bMultiMessages = true;
	NETinit(war_getHostConnectionProvider());
	// Unlike "Host Game", don't set up router port mapping (UPnP): it is not needed on a local network.
	game.type = LEVEL_TYPE::SKIRMISH;

	if (game.name[0] == '\0')
	{
		sstrcpy(game.name, sPlayer[0] != '\0' ? sPlayer : "Local Player");
	}

	NETsetLanOnlyHosting(true); // keep this room off the public lobby
	bBroadcastRoom = true;      // announce it on the local network
	bAutoHostRequest = true;    // and open it right away: no extra "Host Game" step

	changeTitleUI(std::make_shared<WzMultiplayerOptionsTitleUI>(wzTitleUICurrent));
}

void WzLocalPlayTitleUI::joinRoom(const LanRoomInfo &room)
{
	debug(LOG_INFO, "Local Player: joining room at %s:%u", room.address.c_str(), static_cast<unsigned>(room.port));
	LanDiscovery::browseStop();
	NETsetLanOnlyHosting(false);

	// Show progress instead of the room list while connecting (the lobby opens once the host accepts us)
	joining = true;
	widgDelete(psWScreen, LOCALPLAY_STATUS);
	for (size_t i = 0; i < MAX_ROOM_BUTTONS; ++i)
	{
		widgDelete(psWScreen, LOCALPLAY_ROOM0 + static_cast<unsigned>(i));
	}
	addTextButton(LOCALPLAY_STATUS, FRONTEND_POS2X, FRONTEND_POS2Y, astringf(_("Connecting to %s..."), truncateForButton(room.hostName, 18).c_str()), WBUT_TXTCENTRE | WBUT_DISABLE);

	joinGameFromIPPort(room.address, room.port);
}

void WzLocalPlayTitleUI::goBack()
{
	if (page == Page::JoinList)
	{
		LanDiscovery::browseStop();
		page = Page::Menu;
		joining = false;
		widgDelete(psWScreen, FRONTEND_BACKDROP);
		showMenuPage();
		return;
	}
	changeTitleUI(parent);
}

TITLECODE WzLocalPlayTitleUI::run()
{
	screen_disableMapPreview();

	WidgetTriggers const &triggers = widgRunScreen(psWScreen);
	const unsigned id = triggers.empty() ? 0 : triggers.front().widget->id; // first click only

	if (id == LOCALPLAY_BACK || CancelPressed())
	{
		goBack();
		return TITLECODE_CONTINUE;
	}
	if (page == Page::Menu)
	{
		if (id == LOCALPLAY_CREATE)
		{
			createRoom();
		}
		else if (id == LOCALPLAY_JOIN)
		{
			showJoinPage();
		}
		return TITLECODE_CONTINUE;
	}

	// Join page
	if (id == LOCALPLAY_REFRESH && !joining)
	{
		LanDiscovery::browseStop(); // closes the socket and forgets old rooms
		LanDiscovery::browseStart();
		rooms.clear();
		roomsSignature.clear();
		joinPageStart = std::chrono::steady_clock::now();
		singleRoomKey.clear();
		widgDelete(psWScreen, LOCALPLAY_HELP);
		rebuildRoomButtons();
		return TITLECODE_CONTINUE;
	}
	if (joining)
	{
		return TITLECODE_CONTINUE; // waiting for the connection; Back cancels the view (handled above)
	}
	if (id >= LOCALPLAY_ROOM0 && id < LOCALPLAY_ROOM0 + MAX_ROOM_BUTTONS)
	{
		const size_t index = id - LOCALPLAY_ROOM0;
		if (index < rooms.size() && rooms[index].compatible)
		{
			const LanRoomInfo room = rooms[index]; // copy: joining changes the UI state
			joinRoom(room);
			return TITLECODE_CONTINUE;
		}
	}

	std::vector<LanRoomInfo> found = LanDiscovery::browsePoll();
	const std::string signature = roomsSignatureOf(found);
	if (signature != roomsSignature)
	{
		roomsSignature = signature;
		rooms = std::move(found);
		rebuildRoomButtons();
	}

	const auto now = std::chrono::steady_clock::now();

	// Auto-join: exactly one joinable room that has stayed visible for a moment
	if (AUTO_JOIN_SINGLE_ROOM)
	{
		const LanRoomInfo *only = nullptr;
		size_t compatibleCount = 0;
		for (const LanRoomInfo &r : rooms)
		{
			if (r.compatible)
			{
				++compatibleCount;
				only = &r;
			}
		}
		if (compatibleCount == 1)
		{
			const std::string key = only->address + ":" + std::to_string(only->port);
			if (key != singleRoomKey)
			{
				singleRoomKey = key;
				singleRoomSince = now;
			}
			else if (now - singleRoomSince >= AUTO_JOIN_DELAY)
			{
				const LanRoomInfo room = *only; // copy: joining changes the UI state
				joinRoom(room);
				return TITLECODE_CONTINUE;
			}
		}
		else
		{
			singleRoomKey.clear();
		}
	}

	// Nothing found for a while: explain what to check
	if (rooms.empty() && now - joinPageStart >= HELP_DELAY && widgGetFromID(psWScreen, LOCALPLAY_HELP) == nullptr)
	{
		addSmallTextButton(LOCALPLAY_HELP, FRONTEND_POS9X, FRONTEND_POS9Y,
			_("No rooms found. Check that both devices use the same Wi-Fi (or the host's hotspot), the host has tapped Create Room, then tap Refresh."), 0);
	}
	else if (!rooms.empty())
	{
		widgDelete(psWScreen, LOCALPLAY_HELP);
	}
	return TITLECODE_CONTINUE;
}

void WzLocalPlayTitleUI::render()
{
	widgDisplayScreen(psWScreen);
}
