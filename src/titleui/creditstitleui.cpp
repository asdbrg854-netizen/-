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
 *  "Thanks" screen (credits).
 */

#include "lib/framework/frame.h"
#include "creditstitleui.h"
#include "credits_data.h"
#include "lib/ivis_opengl/textdraw.h"
#include "lib/widget/widget.h"
#include "lib/widget/button.h"
#include "lib/widget/label.h"
#include "lib/widget/scrollablelist.h"
#include "../frontend.h"
#include "../frend.h"
#include "../hci.h"
#include "../intdisplay.h"

#include <iterator>

namespace
{
constexpr unsigned CREDITS_BACK = 4598300;
constexpr int LIST_TOP = 49;      // below the Return button row
constexpr int LIST_PADDING = 12;
}

WzCreditsTitleUI::WzCreditsTitleUI(std::shared_ptr<WzTitleUI> parent)
	: parent(parent)
{
}

WzCreditsTitleUI::~WzCreditsTitleUI()
{
}

void WzCreditsTitleUI::start()
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

	addSideText(FRONTEND_SIDETEXT, FRONTEND_SIDEX, FRONTEND_TOPFORM_WIDEY, _("THANKS"));
	addMultiBut(psWScreen, FRONTEND_BOTFORM, CREDITS_BACK, 10, 10, 30, 29, P_("menu", "Return"), IMAGE_RETURN, IMAGE_RETURN_HI, IMAGE_RETURN_HI);

	auto list = ScrollableListWidget::make();
	botForm->attach(list);
	list->setCalcLayout([](WIDGET *psWidget) {
		WIDGET *psParent = psWidget->parent();
		ASSERT_OR_RETURN(, psParent != nullptr, "No parent?");
		psWidget->setGeometry(LIST_PADDING, LIST_TOP, psParent->width() - 2 * LIST_PADDING, psParent->height() - LIST_TOP - LIST_PADDING);
	});
	list->setItemSpacing(2);

	for (const CreditLine &line : kCreditLines)
	{
		auto label = std::make_shared<W_LABEL>();
		label->setGeometry(0, 0, 540, line.style == 0 ? 34 : (line.style == 1 ? 26 : (line.style == 3 ? 10 : 18)));
		switch (line.style)
		{
		case 0:
			label->setFont(font_large, WZCOL_TEXT_BRIGHT);
			break;
		case 1:
			label->setFont(font_medium_bold, WZCOL_YELLOW);
			break;
		default:
			label->setFont(font_small, WZCOL_TEXT_MEDIUM);
			break;
		}
		label->setString(WzString::fromUtf8(line.text));
		list->addItem(label);
	}
}

TITLECODE WzCreditsTitleUI::run()
{
	WidgetTriggers const &triggers = widgRunScreen(psWScreen);
	const unsigned id = triggers.empty() ? 0 : triggers.front().widget->id;

	if (id == CREDITS_BACK || CancelPressed())
	{
		changeTitleUI(parent);
	}
	return TITLECODE_CONTINUE;
}

void WzCreditsTitleUI::render()
{
	widgDisplayScreen(psWScreen);
}
