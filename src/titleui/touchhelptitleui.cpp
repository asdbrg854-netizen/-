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
 *  Touch controls help screen.
 */

#include "lib/framework/frame.h"
#include "touchhelptitleui.h"
#include "lib/framework/physfs_ext.h"
#include "lib/ivis_opengl/textdraw.h"
#include "lib/widget/widget.h"
#include "lib/widget/button.h"
#include "lib/widget/label.h"
#include "lib/widget/scrollablelist.h"
#include "../frontend.h"
#include "../frend.h"
#include "../hci.h"
#include "../intdisplay.h"

namespace
{
constexpr unsigned TOUCHHELP_BACK = 4598310;
constexpr int LIST_TOP = 49;
constexpr int LIST_PADDING = 12;
constexpr const char *kMarkerFile = "touch_help_shown.txt";

struct HelpLine { int style; const char *text; }; // style: 0 title, 1 section, 2 text, 3 blank

// Keep every line short (about 70 characters): the list is 540 pixels wide.
const HelpLine kHelpLines[] = {
	{0, N_("Touch Controls")},
	{3, ""},
	{1, N_("Select and command")},
	{2, N_("Tap a unit or a structure to select it.")},
	{2, N_("Tap empty ground to deselect.")},
	{2, N_("Press and hold for a moment (the phone vibrates), then lift")},
	{2, N_("your finger: this gives a command - move, attack, repair.")},
	{2, N_("Press and hold, then drag: select several units with a box.")},
	{3, ""},
	{1, N_("Camera")},
	{2, N_("Use two fingers: pinch to zoom, twist to rotate, slide to move.")},
	{3, ""},
	{1, N_("Building")},
	{2, N_("1. Tap a truck.   2. Open the build menu.")},
	{2, N_("3. Choose a structure.   4. Tap where it should go.")},
	{2, N_("A busy truck keeps its job: a free truck is sent to build")},
	{2, N_("the new structure. If no truck is free, it waits in the queue.")},
	{3, ""},
	{1, N_("Local Player")},
	{2, N_("Tap Create Room on one phone and Join Room on the others.")},
	{2, N_("All phones must use the same Wi-Fi or the host's hotspot.")},
};
}

bool wzTouchHelpAlreadyShown()
{
	return PHYSFS_exists(kMarkerFile) != 0;
}

void wzTouchHelpMarkShown()
{
	PHYSFS_file *file = PHYSFS_openWrite(kMarkerFile);
	if (file != nullptr)
	{
		const char text[] = "shown\n";
		WZ_PHYSFS_writeBytes(file, text, sizeof(text) - 1);
		PHYSFS_close(file);
	}
}

WzTouchHelpTitleUI::WzTouchHelpTitleUI(std::shared_ptr<WzTitleUI> parent)
	: parent(parent)
{
}

WzTouchHelpTitleUI::~WzTouchHelpTitleUI()
{
}

void WzTouchHelpTitleUI::start()
{
	addBackdrop();

	WIDGET *backdrop = widgGetFromID(psWScreen, FRONTEND_BACKDROP);
	ASSERT_OR_RETURN(, backdrop != nullptr, "Unable to find FRONTEND_BACKDROP?");

	auto botForm = std::make_shared<IntFormAnimated>();
	backdrop->attach(botForm);
	botForm->id = FRONTEND_BOTFORM;
	botForm->setCalcLayout(LAMBDA_CALCLAYOUT_SIMPLE({
		psWidget->setGeometry(FRONTEND_BOTFORM_WIDEX, FRONTEND_TOPFORM_WIDEY, 590, 460);
	}));

	addSideText(FRONTEND_SIDETEXT, FRONTEND_SIDEX, FRONTEND_TOPFORM_WIDEY, _("CONTROLS"));
	addMultiBut(psWScreen, FRONTEND_BOTFORM, TOUCHHELP_BACK, 10, 10, 30, 29, P_("menu", "Return"), IMAGE_RETURN, IMAGE_RETURN_HI, IMAGE_RETURN_HI);

	auto list = ScrollableListWidget::make();
	botForm->attach(list);
	list->setCalcLayout([](WIDGET *psWidget) {
		WIDGET *psParent = psWidget->parent();
		ASSERT_OR_RETURN(, psParent != nullptr, "No parent?");
		psWidget->setGeometry(LIST_PADDING, LIST_TOP, psParent->width() - 2 * LIST_PADDING, psParent->height() - LIST_TOP - LIST_PADDING);
	});
	list->setItemSpacing(3);

	for (const HelpLine &line : kHelpLines)
	{
		auto label = std::make_shared<W_LABEL>();
		label->setGeometry(0, 0, 540, line.style == 0 ? 34 : (line.style == 1 ? 26 : (line.style == 3 ? 10 : 20)));
		switch (line.style)
		{
		case 0:
			label->setFont(font_large, WZCOL_TEXT_BRIGHT);
			break;
		case 1:
			label->setFont(font_medium_bold, WZCOL_YELLOW);
			break;
		default:
			label->setFont(font_small, WZCOL_TEXT_BRIGHT);
			break;
		}
		label->setString(line.text[0] != '\0' ? WzString::fromUtf8(gettext(line.text)) : WzString());
		list->addItem(label);
	}
}

TITLECODE WzTouchHelpTitleUI::run()
{
	WidgetTriggers const &triggers = widgRunScreen(psWScreen);
	const unsigned id = triggers.empty() ? 0 : triggers.front().widget->id;

	if (id == TOUCHHELP_BACK || CancelPressed())
	{
		changeTitleUI(parent);
	}
	return TITLECODE_CONTINUE;
}

void WzTouchHelpTitleUI::render()
{
	widgDisplayScreen(psWScreen);
}
