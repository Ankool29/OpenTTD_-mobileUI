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
#include "../viewport_func.h"
#include "../window_gui.h"

#include "../safeguards.h"

TEST_CASE("Placement - cancel resets geometry and arms a valid preview")
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
	CHECK(_thd.size.x == TILE_SIZE);
	CHECK(_thd.size.y == TILE_SIZE);
	CHECK(_thd.offs.x == 0);
	CHECK(_thd.offs.y == 0);
	CHECK(_thd.outersize.x == 0);
	CHECK(_thd.outersize.y == 0);
	CHECK_FALSE(_thd.diagonal);
	CHECK(_thd.new_pos.x == 0);
	CHECK(_thd.new_pos.y == 0);
	CHECK(_thd.new_size.x == TILE_SIZE);
	CHECK(_thd.new_size.y == TILE_SIZE);
	CHECK(_thd.new_outersize.x == 0);
	CHECK(_thd.new_outersize.y == 0);
	CHECK(_thd.selstart.x == 0);
	CHECK(_thd.selstart.y == 0);
	CHECK(_thd.selend.x == 0);
	CHECK(_thd.selend.y == 0);
	CHECK(_thd.place_mode == HT_RECT);
}

TEST_CASE("Placement - cancel restores a valid preview from empty line geometry")
{
	Map::Allocate(256, 256);

	/* This is the geometry reported by the Android diagnostic after the
	 * second finger cancelled an autorail drag: place=draw=HT_LINE|HT_DIR_Y
	 * with pos=(1840, 2064) and size=(0, 0). */
	_special_mouse_mode = SpecialMouseMode::Sizing;
	_thd.place_mode = HT_LINE | HT_DIR_Y;
	_thd.next_drawstyle = HT_LINE | HT_DIR_Y;
	_thd.drawstyle = HT_LINE | HT_DIR_Y;
	_thd.select_method = VPM_FIX_X | VPM_RAILDIRS;
	_thd.pos = {1840, 2064};
	_thd.size = {0, 0};
	_thd.new_pos = {1840, 2064};
	_thd.new_size = {0, 0};
	_thd.offs = {0, 0};
	_thd.outersize = {0, 0};
	_thd.new_outersize = {0, 0};
	_thd.diagonal = false;
	_thd.selstart = {1840, 2064};
	_thd.selend = {1840, 2064};

	VpCancelPlaceSizing();

	CHECK(_special_mouse_mode == SpecialMouseMode::None);
	CHECK(_thd.place_mode == (HT_LINE | HT_DIR_Y));
	CHECK(_thd.drawstyle == HT_NONE);
	CHECK(_thd.size.x >= TILE_SIZE);
	CHECK(_thd.size.y >= TILE_SIZE);
	CHECK(_thd.new_size.x >= TILE_SIZE);
	CHECK(_thd.new_size.y >= TILE_SIZE);
	CHECK(_thd.pos.x == 0);
	CHECK(_thd.pos.y == 0);
	CHECK(_thd.new_pos.x == 0);
	CHECK(_thd.new_pos.y == 0);
	CHECK(_thd.selstart.x == 0);
	CHECK(_thd.selstart.y == 0);
	CHECK(_thd.selend.x == 0);
	CHECK(_thd.selend.y == 0);
}

TEST_CASE("Placement - empty selection is not marked dirty")
{
	Map::Allocate(256, 256);

	/* Rotation, tool changes and focus loss can call this with no valid
	 * rectangle left. It must not calculate tile indices in that state. */
	_thd.pos = {1840, 2064};
	_thd.size = {0, 0};
	_thd.offs = {0, 0};
	_thd.outersize = {0, 0};
	_thd.diagonal = false;
	_thd.drawstyle = HT_NONE;

	SetSelectionRed(true);

	CHECK(_thd.size.x == 0);
	CHECK(_thd.size.y == 0);
}
