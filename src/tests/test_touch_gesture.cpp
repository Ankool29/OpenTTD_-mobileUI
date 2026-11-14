/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file test_touch_gesture.cpp Tests for the platform-independent touch gesture state machine. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../video/touch_gesture_logic.h"

#include "../safeguards.h"

TEST_CASE("TouchGesture - tap produces one left click")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	const TouchGestureActions down = logic.HandleDown(1, 0.25f, 0.25f, 0);
	CHECK(down.move_cursor);
	CHECK_FALSE(down.press_left);
	CHECK(logic.GetPhase() == TouchGesturePhase::PrimaryDown);

	const TouchGestureActions up = logic.HandleUp(1, 0.25f, 0.25f, 100);
	CHECK(up.click_left);
	CHECK_FALSE(up.press_left);
	CHECK(logic.GetPhase() == TouchGesturePhase::Idle);
	CHECK(logic.GetFingerCount() == 0);
}

TEST_CASE("TouchGesture - drag starts after the threshold and releases the left button")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.20f, 0.20f, 0);
	const TouchGestureActions below = logic.HandleMotion(1, 0.205f, 0.20f);
	CHECK_FALSE(below.press_left);
	CHECK(logic.GetPhase() == TouchGesturePhase::PrimaryDown);

	const TouchGestureActions start = logic.HandleMotion(1, 0.22f, 0.20f);
	CHECK(start.move_cursor);
	CHECK(start.reset_delta);
	CHECK(start.press_left);
	CHECK(logic.GetPhase() == TouchGesturePhase::PrimaryDragging);

	const TouchGestureActions move = logic.HandleMotion(1, 0.25f, 0.25f);
	CHECK(move.move_cursor);
	CHECK(move.update_mouse);
	CHECK_FALSE(move.press_left);

	const TouchGestureActions up = logic.HandleUp(1, 0.25f, 0.25f, 100);
	CHECK(up.release_left);
	CHECK_FALSE(up.click_left);
	CHECK(logic.GetPhase() == TouchGesturePhase::Idle);
	CHECK(logic.GetFingerCount() == 0);
}

TEST_CASE("TouchGesture - long press produces a right click and no tap")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.50f, 0.50f, 1000);
	CHECK_FALSE(logic.Tick(1499).click_right);
	const TouchGestureActions long_press = logic.Tick(1500);
	CHECK(long_press.click_right);
	CHECK(logic.IsPrimaryLongPress());

	const TouchGestureActions up = logic.HandleUp(1, 0.50f, 0.50f, 1600);
	CHECK_FALSE(up.click_left);
	CHECK(logic.GetPhase() == TouchGesturePhase::Idle);
}

TEST_CASE("TouchGesture - two fingers use wheel events without pressing the left button")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.40f, 0.50f, 0);
	logic.HandleDown(2, 0.60f, 0.50f, 10);
	CHECK(logic.GetPhase() == TouchGesturePhase::TwoFinger);

	const TouchGestureActions pinch = logic.HandleMotion(2, 0.80f, 0.50f);
	CHECK(pinch.wheel_steps == -1);
	CHECK_FALSE(pinch.press_left);
	CHECK_FALSE(pinch.release_left);
}

TEST_CASE("TouchGesture - second finger during a drag releases the left button")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.20f, 0.20f, 0);
	REQUIRE(logic.HandleMotion(1, 0.22f, 0.20f).press_left);
	CHECK(logic.GetPhase() == TouchGesturePhase::PrimaryDragging);

	const TouchGestureActions second_down = logic.HandleDown(2, 0.70f, 0.70f, 20);
	CHECK(second_down.release_left);
	CHECK(second_down.reset_delta);
	CHECK(logic.GetPhase() == TouchGesturePhase::TwoFinger);
}

TEST_CASE("TouchGesture - extra fingers and unknown releases do not corrupt the gesture")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.20f, 0.20f, 0);
	logic.HandleDown(2, 0.40f, 0.40f, 10);
	CHECK(logic.GetFingerCount() == 2);

	const TouchGestureActions third_down = logic.HandleDown(3, 0.60f, 0.60f, 20);
	CHECK_FALSE(third_down.press_left);
	CHECK(logic.GetFingerCount() == 2);

	const TouchGestureActions third_up = logic.HandleUp(3, 0.60f, 0.60f, 30);
	CHECK_FALSE(third_up.release_left);
	CHECK(logic.GetFingerCount() == 2);

	const TouchGestureActions first_up = logic.HandleUp(1, 0.20f, 0.20f, 40);
	CHECK_FALSE(first_up.click_left);
	CHECK(logic.GetFingerCount() == 1);
	CHECK(logic.GetPhase() == TouchGesturePhase::PrimaryDown);

	const TouchGestureActions second_up = logic.HandleUp(2, 0.40f, 0.40f, 50);
	CHECK_FALSE(second_up.click_left);
	CHECK(logic.GetFingerCount() == 0);
	CHECK(logic.GetPhase() == TouchGesturePhase::Idle);
}

TEST_CASE("TouchGesture - duplicate finger down is ignored")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.20f, 0.20f, 0);
	const TouchGestureActions duplicate = logic.HandleDown(1, 0.80f, 0.80f, 10);
	CHECK_FALSE(duplicate.move_cursor);
	CHECK(logic.GetFingerCount() == 1);
	CHECK(logic.GetPhase() == TouchGesturePhase::PrimaryDown);
}

TEST_CASE("TouchGesture - interruption releases a drag without leaving a button pressed")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.20f, 0.20f, 0);
	REQUIRE(logic.HandleMotion(1, 0.25f, 0.20f).press_left);

	const TouchGestureActions reset = logic.Reset();
	CHECK(reset.release_left);
	CHECK(reset.reset_delta);
	CHECK(logic.GetFingerCount() == 0);
	CHECK(logic.GetPhase() == TouchGesturePhase::Idle);
}

TEST_CASE("TouchGesture - screen size changes remap normalised coordinates")
{
	TouchGestureLogic logic;

	logic.SetScreenSize(100, 200);
	CHECK(logic.TouchToScreen(0.50f, 0.50f) == std::pair<int, int>(50, 100));

	logic.SetScreenSize(200, 100);
	CHECK(logic.TouchToScreen(0.50f, 0.50f) == std::pair<int, int>(100, 50));

	CHECK(logic.TouchToScreen(-1.0f, 2.0f) == std::pair<int, int>(0, 99));
}

TEST_CASE("TouchGesture - screen size changes do not reset an active gesture")
{
	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);

	logic.HandleDown(1, 0.50f, 0.50f, 0);
	logic.SetScreenSize(2000, 500);

	CHECK(logic.GetFingerCount() == 1);
	CHECK(logic.GetPhase() == TouchGesturePhase::PrimaryDown);
	CHECK(logic.TouchToScreen(0.50f, 0.50f) == std::pair<int, int>(1000, 250));
}
