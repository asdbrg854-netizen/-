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
 *  "Local Player": play with friends on the same Wi-Fi (or phone hotspot) without any internet lobby.
 *  Create Room hosts a game and announces it on the local network; Join Room lists the rooms found.
 */

#ifndef __INCLUDED_SRC_TITLEUI_LOCALPLAY_H__
#define __INCLUDED_SRC_TITLEUI_LOCALPLAY_H__

#include "titleui.h"
#include "lib/netplay/lan_discovery.h"

#include <memory>
#include <string>
#include <vector>

class WzLocalPlayTitleUI : public WzTitleUI
{
public:
	explicit WzLocalPlayTitleUI(std::shared_ptr<WzTitleUI> parent);
	virtual ~WzLocalPlayTitleUI();
	virtual void start() override;
	virtual TITLECODE run() override;
	virtual void render() override;

private:
	enum class Page
	{
		Menu,     // Create Room / Join Room
		JoinList  // rooms found on the local network
	};

	void showMenuPage();
	void showJoinPage();
	void rebuildRoomButtons();
	void createRoom();
	void joinRoom(const LanRoomInfo &room);
	void goBack();

	std::shared_ptr<WzTitleUI> parent;
	Page page = Page::Menu;
	std::vector<LanRoomInfo> rooms;
	std::string roomsSignature;
	bool joining = false; // a join attempt is in progress: stop refreshing the list
};

// Hooks used by the multiplayer lobby UI (multiint.cpp):

/// True once after "Create Room": the lobby should start hosting immediately instead of waiting for the Host button.
bool localPlayConsumeAutoHostRequest();
/// Call every frame while the lobby is shown. Announces the room on the local network while it is being hosted.
void localPlayHostTick();
/// Call when the lobby UI goes away: stops announcing the room.
void localPlayHostStop();
/// Call when starting a normal (internet) hosted game, so Local Player settings don't carry over.
void localPlayReset();

#endif
