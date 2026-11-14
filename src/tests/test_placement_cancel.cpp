/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file test_placement_cancel.cpp Tests for cancelling an active placement drag. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../tilehighlight_func.h"
#include "../window_gui.h"

#include "../safeguards.h"

TEST_CASE("Placement - cancel clears all tile highlight geometry")
{
	Map::Allocate(64, 64);

	_special_mouse_mode = SpecialMouseMode::Sizing;
	_thd.place_mode = HT_RECT;
	_thd.next_drawstyle = HT_RECT;
	_thd.drawstyle = HT_RECT;
	_thd.pos = {16, 16};
	_thd.size = {16, 16};
	_thd.offs = {8, 8};
	_thd.outersize = {16, 16};
	_thd.diagonal = true;
	_thd.new_pos = {16, 16};
	_thd.new_size = {16, 16};
	_thd.new_outersize = {16, 16};
	_thd.selstart = {16, 16};
	_thd.selend = {32, 32};

	VpCancelPlaceSizing();

	CHECK(_special_mouse_mode == SpecialMouseMode::None);
	CHECK(_thd.drawstyle == HT_NONE);
	CHECK(_thd.pos.x == 0);
	CHECK(_thd.pos.y == 0);
	CHECK(_thd.size.x == 0);
	CHECK(_thd.size.y == 0);
	CHECK(_thd.offs.x == 0);
	CHECK(_thd.offs.y == 0);
	CHECK(_thd.outersize.x == 0);
	CHECK(_thd.outersize.y == 0);
	CHECK_FALSE(_thd.diagonal);
	CHECK(_thd.new_pos.x == 0);
	CHECK(_thd.new_pos.y == 0);
	CHECK(_thd.new_size.x == 0);
	CHECK(_thd.new_size.y == 0);
	CHECK(_thd.new_outersize.x == 0);
	CHECK(_thd.new_outersize.y == 0);
	CHECK(_thd.selstart.x == 0);
	CHECK(_thd.selstart.y == 0);
	CHECK(_thd.selend.x == 0);
	CHECK(_thd.selend.y == 0);
	CHECK(_thd.place_mode == HT_RECT);
}
