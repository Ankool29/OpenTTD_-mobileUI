/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file touch_gesture.cpp Translation of touch input into OpenTTD mouse input. */

#include "../stdafx.h"
#include "../debug.h"
#include "../gfx_func.h"
#include "../core/math_func.hpp"
#include "../zoom_func.h"
#include "touch_gesture.h"
#include "touch_gesture_logic.h"

#include <cmath>

#include "../safeguards.h"

void TouchGestureLogic::SetScreenSize(int width, int height)
{
	this->screen_width = width;
	this->screen_height = height;
}

std::pair<int, int> TouchGestureLogic::TouchToScreen(float x, float y) const
{
	return {
		Clamp(static_cast<int>(x * this->screen_width), 0, std::max(0, this->screen_width - 1)),
		Clamp(static_cast<int>(y * this->screen_height), 0, std::max(0, this->screen_height - 1)),
	};
}

size_t TouchGestureLogic::FindFinger(uint64_t id) const
{
	for (size_t i = 0; i < this->finger_count; i++) {
		if (this->fingers[i].id == id) return i;
	}
	return this->finger_count;
}

void TouchGestureLogic::UpdateTwoFingerBaseline()
{
	if (this->finger_count < 2) return;
	const Finger &a = this->fingers[0];
	const Finger &b = this->fingers[1];
	this->two_finger_last_distance = std::hypot(b.x - a.x, b.y - a.y);
	this->two_finger_last_centroid_x = (a.x + b.x) / 2.0f;
	this->two_finger_last_centroid_y = (a.y + b.y) / 2.0f;
}

TouchGestureActions TouchGestureLogic::HandleDown(uint64_t id, float x, float y, uint32_t now)
{
	TouchGestureActions actions;

	if (this->FindFinger(id) != this->finger_count) return actions; // Duplicate FINGERDOWN.
	if (this->finger_count >= MAX_FINGERS) return actions; // Extra fingers are tracked as ignored by the caller.

	Finger &finger = this->fingers[this->finger_count++];
	finger.id = id;
	finger.x = x;
	finger.y = y;

	if (this->finger_count == 1) {
		/* Defer the left click until we know it is not a long press or a drag. */
		this->phase = TouchGesturePhase::PrimaryDown;
		this->primary_long_press = false;
		this->two_finger_zoom = false;
		this->two_finger_accumulated = 0.0f;
		this->primary_down_time = now;
		this->primary_start_x = x;
		this->primary_start_y = y;

		actions.move_cursor = true;
		actions.cursor_x = x;
		actions.cursor_y = y;
		actions.reset_delta = true;
		return actions;
	}

	/* A second finger arrived: cancel the tap/long press and start a two-finger gesture. */
	if (this->phase == TouchGesturePhase::PrimaryDragging) {
		actions.release_left = true;
	}
	this->primary_long_press = true;
	this->two_finger_zoom = false;
	this->two_finger_accumulated = 0.0f;
	this->phase = TouchGesturePhase::TwoFinger;
	actions.reset_delta = true;
	this->UpdateTwoFingerBaseline();
	this->two_finger_start_distance = this->two_finger_last_distance;
	return actions;
}

TouchGestureActions TouchGestureLogic::HandleMotion(uint64_t id, float x, float y)
{
	TouchGestureActions actions;

	const size_t index = this->FindFinger(id);
	if (index == this->finger_count) return actions;
	this->fingers[index].x = x;
	this->fingers[index].y = y;

	if (this->finger_count >= 2) {
		const Finger &a = this->fingers[0];
		const Finger &b = this->fingers[1];
		const float distance = std::hypot(b.x - a.x, b.y - a.y);
		const float centroid_x = (a.x + b.x) / 2.0f;
		const float centroid_y = (a.y + b.y) / 2.0f;

		const float distance_delta = distance - this->two_finger_last_distance;
		const float centroid_delta_x = centroid_x - this->two_finger_last_centroid_x;
		const float centroid_delta_y = centroid_y - this->two_finger_last_centroid_y;

		/* Decide once whether this is a pinch (distance) or a two-finger drag (centroid). */
		if (this->two_finger_accumulated == 0.0f) {
			if (std::fabs(distance_delta) < 0.002f && std::hypot(centroid_delta_x, centroid_delta_y) < 0.002f) return actions;
			this->two_finger_zoom = std::fabs(distance_delta) >= std::hypot(centroid_delta_x, centroid_delta_y);
		}

		if (this->two_finger_zoom) {
			this->two_finger_accumulated += distance_delta;
			/* A pinch out (distance grows) means zoom in, which is a negative wheel. */
			if (std::fabs(this->two_finger_accumulated) >= 0.02f) {
				actions.wheel_steps = this->two_finger_accumulated > 0.0f ? -1 : 1;
				this->two_finger_accumulated = 0.0f;
			}
		} else {
			this->two_finger_accumulated += centroid_delta_y;
			if (std::fabs(this->two_finger_accumulated) >= 0.02f) {
				actions.wheel_steps = this->two_finger_accumulated > 0.0f ? 1 : -1;
				this->two_finger_accumulated = 0.0f;
			}
		}

		this->two_finger_last_distance = distance;
		this->two_finger_last_centroid_x = centroid_x;
		this->two_finger_last_centroid_y = centroid_y;
		return actions;
	}

	if (this->primary_long_press) return actions;

	if (this->phase == TouchGesturePhase::PrimaryDown) {
		const float dx = (this->fingers[0].x - this->primary_start_x) * this->screen_width;
		const float dy = (this->fingers[0].y - this->primary_start_y) * this->screen_height;
		if (std::hypot(dx, dy) < this->drag_threshold) return actions;

		this->phase = TouchGesturePhase::PrimaryDragging;
		actions.move_cursor = true;
		actions.cursor_x = this->fingers[0].x;
		actions.cursor_y = this->fingers[0].y;
		actions.reset_delta = true;
		actions.press_left = true;
		return actions;
	}

	if (this->phase == TouchGesturePhase::PrimaryDragging) {
		/* PrimaryDragging: report the movement to the mouse loop. */
		actions.move_cursor = true;
		actions.cursor_x = this->fingers[0].x;
		actions.cursor_y = this->fingers[0].y;
		actions.update_mouse = true;
		return actions;
	}

	return actions;
}

TouchGestureActions TouchGestureLogic::HandleUp(uint64_t id, float x, float y, uint32_t now)
{
	TouchGestureActions actions;

	size_t index = this->FindFinger(id);
	if (index == this->finger_count) return actions;

	if (this->finger_count >= 2) {
		/* End of the two-finger gesture; keep the remaining finger as a plain pointer. */
		for (size_t i = index; i + 1 < this->finger_count; ++i) this->fingers[i] = this->fingers[i + 1];
		--this->finger_count;
		this->two_finger_zoom = false;
		this->two_finger_accumulated = 0.0f;
		if (this->finger_count == 1) {
			this->phase = TouchGesturePhase::PrimaryDown;
			this->primary_down_time = now;
			this->primary_start_x = this->fingers[0].x;
			this->primary_start_y = this->fingers[0].y;
			this->primary_long_press = true; /* Do not trigger a new tap when lifting the last finger. */
		} else {
			this->phase = TouchGesturePhase::Idle;
		}
		return actions;
	}

	if (this->phase == TouchGesturePhase::PrimaryDragging) {
		actions.release_left = true;
	} else if (!this->primary_long_press) {
		actions.move_cursor = true;
		actions.cursor_x = x;
		actions.cursor_y = y;
		actions.click_left = true;
	}

	this->finger_count = 0;
	this->phase = TouchGesturePhase::Idle;
	this->primary_long_press = false;
	this->two_finger_zoom = false;
	this->two_finger_accumulated = 0.0f;
	return actions;
}

TouchGestureActions TouchGestureLogic::Tick(uint32_t now)
{
	TouchGestureActions actions;

	if (this->phase != TouchGesturePhase::PrimaryDown || this->finger_count != 1 || this->primary_long_press) return actions;
	if (now - this->primary_down_time < LONG_PRESS_MS) return actions;

	/* Long press: do not select anything, open the contextual action instead. */
	this->primary_long_press = true;
	actions.move_cursor = true;
	actions.cursor_x = this->fingers[0].x;
	actions.cursor_y = this->fingers[0].y;
	actions.click_right = true;
	return actions;
}

TouchGestureActions TouchGestureLogic::Reset()
{
	TouchGestureActions actions;
	if (this->phase == TouchGesturePhase::PrimaryDragging) actions.release_left = true;
	actions.reset_delta = true;

	this->finger_count = 0;
	this->phase = TouchGesturePhase::Idle;
	this->primary_long_press = false;
	this->two_finger_zoom = false;
	this->two_finger_accumulated = 0.0f;
	return actions;
}

static const char *TouchGesturePhaseName(TouchGesturePhase phase)
{
	switch (phase) {
		case TouchGesturePhase::Idle: return "idle";
		case TouchGesturePhase::PrimaryDown: return "primary-down";
		case TouchGesturePhase::PrimaryDragging: return "primary-dragging";
		case TouchGesturePhase::TwoFinger: return "two-finger";
		default: return "unknown";
	}
}

void TouchGestureHandler::SetScreenSize(int width, int height)
{
	if (width == this->logic.GetScreenWidth() && height == this->logic.GetScreenHeight()) return;

	Debug(Facility::Driver, Severity::Debug1, "touch: screen size {}x{} -> {}x{}",
			this->logic.GetScreenWidth(), this->logic.GetScreenHeight(), width, height);
	this->logic.SetScreenSize(width, height);
}

void TouchGestureHandler::Tick()
{
	this->logic.SetDragThreshold(static_cast<float>(ScaleGUITrad(10)));
	const TouchGestureActions actions = this->logic.Tick(SDL_GetTicks());
	this->ApplyActions(actions);
	this->LogStateChange("tick");
}

bool TouchGestureHandler::HandleEvent(const SDL_Event &ev)
{
	this->logic.SetDragThreshold(static_cast<float>(ScaleGUITrad(10)));

	if (ev.type == SDL_FINGERDOWN) {
		const uint64_t id = static_cast<uint64_t>(ev.tfinger.fingerId);
		if (this->logic.GetFingerCount() >= TouchGestureLogic::MAX_FINGERS) {
			Debug(Facility::Driver, Severity::Debug1, "touch: ignoring extra finger {}", id);
			return true;
		}

		const TouchGestureActions actions = this->logic.HandleDown(id, ev.tfinger.x, ev.tfinger.y, SDL_GetTicks());
		this->ApplyActions(actions);
		this->LogStateChange("finger-down");
		return true;
	}

	if (ev.type == SDL_FINGERMOTION) {
		const uint64_t id = static_cast<uint64_t>(ev.tfinger.fingerId);
		const TouchGestureActions actions = this->logic.HandleMotion(id, ev.tfinger.x, ev.tfinger.y);
		this->ApplyActions(actions);
		this->LogStateChange("finger-motion");
		return true;
	}

	if (ev.type == SDL_FINGERUP) {
		const uint64_t id = static_cast<uint64_t>(ev.tfinger.fingerId);
		const TouchGestureActions actions = this->logic.HandleUp(id, ev.tfinger.x, ev.tfinger.y, SDL_GetTicks());
		this->ApplyActions(actions);
		this->LogStateChange("finger-up");
		return true;
	}

	return false;
}

void TouchGestureHandler::Cancel()
{
	const TouchGestureActions actions = this->logic.Reset();
	this->ApplyActions(actions);
	this->LogStateChange("cancel");
}

void TouchGestureHandler::ApplyActions(const TouchGestureActions &actions)
{
	if (actions.move_cursor) this->MoveCursor(actions.cursor_x, actions.cursor_y);
	if (actions.reset_delta) {
		_cursor.delta.x = 0;
		_cursor.delta.y = 0;
	}
	if (actions.press_left) this->PressLeft();
	if (actions.release_left) this->ReleaseLeft();
	if (actions.click_left) this->ClickLeft();
	if (actions.click_right) this->ClickRight();
	if (actions.wheel_steps != 0) this->Wheel(actions.wheel_steps);
	if (actions.update_mouse) HandleMouseEvents();
}

void TouchGestureHandler::LogStateChange(const char *event)
{
	const TouchGesturePhase phase = this->logic.GetPhase();
	const size_t finger_count = this->logic.GetFingerCount();
	const bool long_press = this->logic.IsPrimaryLongPress();
	const bool two_finger_zoom = this->logic.IsTwoFingerZoom();

	if (phase != this->last_phase || finger_count != this->last_finger_count ||
			long_press != this->last_long_press || two_finger_zoom != this->last_two_finger_zoom) {
		Debug(Facility::Driver, Severity::Debug1, "touch: {}: phase {} -> {}, fingers {} -> {}, long-press {} -> {}, zoom {} -> {}",
				event, TouchGesturePhaseName(this->last_phase), TouchGesturePhaseName(phase),
				this->last_finger_count, finger_count, this->last_long_press, long_press,
				this->last_two_finger_zoom, two_finger_zoom);
	}

	this->last_phase = phase;
	this->last_finger_count = finger_count;
	this->last_long_press = long_press;
	this->last_two_finger_zoom = two_finger_zoom;
}

void TouchGestureHandler::MoveCursor(float x, float y)
{
	const auto [screen_x, screen_y] = this->logic.TouchToScreen(x, y);
	_cursor.UpdateCursorPosition(screen_x, screen_y);
}

void TouchGestureHandler::PressLeft()
{
	_left_button_down = true;
	_left_button_clicked = false;
	HandleMouseEvents();
}

void TouchGestureHandler::ReleaseLeft()
{
	_left_button_down = false;
	_left_button_clicked = false;
	HandleMouseEvents();
}

void TouchGestureHandler::ClickLeft()
{
	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
	this->PressLeft();
	this->ReleaseLeft();
}

void TouchGestureHandler::ClickRight()
{
	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
	_right_button_down = true;
	_right_button_clicked = true;
	HandleMouseEvents();
	_right_button_down = false;
}

void TouchGestureHandler::Wheel(int steps)
{
	if (steps == 0) return;
	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
	_cursor.wheel += steps;
	HandleMouseEvents();
}
