/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/gpl-2.0>.
 */

/** @file touch_gesture.cpp Translation of touch input into OpenTTD mouse input. */

#include "../stdafx.h"
#include "../gfx_func.h"
#include "../core/math_func.hpp"
#include "../zoom_func.h"
#include "touch_gesture.h"

#include <cmath>

#include "../safeguards.h"

void TouchGestureHandler::SetScreenSize(int width, int height)
{
	this->screen_width = width;
	this->screen_height = height;
}

Point TouchGestureHandler::TouchToScreen(float x, float y) const
{
	return {
		Clamp(static_cast<int>(x * this->screen_width), 0, std::max(0, this->screen_width - 1)),
		Clamp(static_cast<int>(y * this->screen_height), 0, std::max(0, this->screen_height - 1)),
	};
}

void TouchGestureHandler::MoveCursor(float x, float y)
{
	const Point pt = this->TouchToScreen(x, y);
	_cursor.UpdateCursorPosition(pt.x, pt.y);
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
	this->PressLeft();
	this->ReleaseLeft();
}

void TouchGestureHandler::ClickRight()
{
	_right_button_down = true;
	_right_button_clicked = true;
	HandleMouseEvents();
	_right_button_down = false;
}

void TouchGestureHandler::Wheel(int steps)
{
	if (steps == 0) return;
	_cursor.wheel += steps;
	HandleMouseEvents();
}

void TouchGestureHandler::ResetPrimary()
{
	this->primary_dragging = false;
	this->primary_long_press = false;
	this->two_finger_gesture = false;
	this->two_finger_zoom = false;
	this->two_finger_accumulated = 0.0f;
}

void TouchGestureHandler::ResetSecondary()
{
	this->two_finger_gesture = false;
	this->two_finger_zoom = false;
	this->two_finger_accumulated = 0.0f;
}

void TouchGestureHandler::UpdateTwoFingerBaseline()
{
	if (this->finger_count < 2) return;
	const Finger &a = this->fingers[0];
	const Finger &b = this->fingers[1];
	this->two_finger_last_distance = std::hypot(b.x - a.x, b.y - a.y);
	this->two_finger_last_centroid_x = (a.x + b.x) / 2.0f;
	this->two_finger_last_centroid_y = (a.y + b.y) / 2.0f;
}

void TouchGestureHandler::Tick()
{
	if (this->finger_count != 1 || this->primary_dragging || this->primary_long_press) return;
	if (SDL_GetTicks() - this->primary_down_time < LONG_PRESS_MS) return;

	/* Long press: do not select anything, open the contextual action instead. */
	this->primary_long_press = true;
	this->MoveCursor(this->fingers[0].x, this->fingers[0].y);
	this->ClickRight();
}

bool TouchGestureHandler::HandleEvent(const SDL_Event &ev)
{
	if (ev.type == SDL_FINGERDOWN) {
		if (this->finger_count >= MAX_FINGERS) return true;

		Finger &f = this->fingers[this->finger_count++];
		f.id = ev.tfinger.fingerId;
		f.x = ev.tfinger.x;
		f.y = ev.tfinger.y;

		if (this->finger_count == 1) {
			/* Defer the left click until we know it is not a long press or a drag. */
			this->ResetPrimary();
			this->primary_down_time = SDL_GetTicks();
			this->primary_start_x = f.x;
			this->primary_start_y = f.y;
			this->MoveCursor(f.x, f.y);
			_cursor.delta.x = 0;
			_cursor.delta.y = 0;
			_cursor.in_window = true;
			return true;
		}

		/* A second finger arrived: cancel the tap/long press and start a two-finger gesture. */
		if (this->primary_dragging) {
			this->ReleaseLeft();
			this->primary_dragging = false;
		}
		this->primary_long_press = true;
		this->two_finger_gesture = true;
		this->two_finger_zoom = false;
		this->two_finger_accumulated = 0.0f;
		this->UpdateTwoFingerBaseline();
		this->two_finger_start_distance = this->two_finger_last_distance;
		return true;
	}

	if (ev.type == SDL_FINGERMOTION) {
		for (size_t i = 0; i < this->finger_count; ++i) {
			if (this->fingers[i].id == ev.tfinger.fingerId) {
				this->fingers[i].x = ev.tfinger.x;
				this->fingers[i].y = ev.tfinger.y;
				break;
			}
		}

		if (this->finger_count >= 2) {
			const Finger &a = this->fingers[0];
			const Finger &b = this->fingers[1];
			const float distance = std::hypot(b.x - a.x, b.y - a.y);
			const float centroid_x = (a.x + b.x) / 2.0f;
			const float centroid_y = (a.y + b.y) / 2.0f;

			if (!this->two_finger_gesture) return true;

			const float distance_delta = distance - this->two_finger_last_distance;
			const float centroid_delta_x = centroid_x - this->two_finger_last_centroid_x;
			const float centroid_delta_y = centroid_y - this->two_finger_last_centroid_y;

			/* Decide once whether this is a pinch (distance) or a two-finger drag (centroid). */
			if (this->two_finger_accumulated == 0.0f) {
				if (std::fabs(distance_delta) < 0.002f && std::hypot(centroid_delta_x, centroid_delta_y) < 0.002f) return true;
				this->two_finger_zoom = std::fabs(distance_delta) >= std::hypot(centroid_delta_x, centroid_delta_y);
			}

			if (this->two_finger_zoom) {
				this->two_finger_accumulated += distance_delta;
				/* A pinch out (distance grows) means zoom in, which is a negative wheel. */
				if (std::fabs(this->two_finger_accumulated) >= 0.02f) {
					const int steps = this->two_finger_accumulated > 0.0f ? -1 : 1;
					this->Wheel(steps);
					this->two_finger_accumulated = 0.0f;
				}
			} else {
				this->two_finger_accumulated += centroid_delta_y;
				if (std::fabs(this->two_finger_accumulated) >= 0.02f) {
					const int steps = this->two_finger_accumulated > 0.0f ? 1 : -1;
					this->Wheel(steps);
					this->two_finger_accumulated = 0.0f;
				}
			}

			this->two_finger_last_distance = distance;
			this->two_finger_last_centroid_x = centroid_x;
			this->two_finger_last_centroid_y = centroid_y;
			return true;
		}

		if (this->finger_count != 1 || this->primary_long_press) return true;

		if (!this->primary_dragging) {
			const float dx = (this->fingers[0].x - this->primary_start_x) * this->screen_width;
			const float dy = (this->fingers[0].y - this->primary_start_y) * this->screen_height;
			const float threshold = static_cast<float>(ScaleGUITrad(10));
			if (std::hypot(dx, dy) < threshold) return true;

			this->primary_dragging = true;
			this->MoveCursor(this->fingers[0].x, this->fingers[0].y);
			_cursor.delta.x = 0;
			_cursor.delta.y = 0;
			this->PressLeft();
			return true;
		}

		this->MoveCursor(this->fingers[0].x, this->fingers[0].y);
		HandleMouseEvents();
		return true;
	}

	if (ev.type == SDL_FINGERUP) {
		size_t index = this->finger_count;
		for (size_t i = 0; i < this->finger_count; ++i) {
			if (this->fingers[i].id == ev.tfinger.fingerId) {
				index = i;
				break;
			}
		}
		if (index == this->finger_count) return true;

		if (this->finger_count >= 2) {
			/* End of the two-finger gesture; keep the remaining finger as a plain pointer. */
			for (size_t i = index; i + 1 < this->finger_count; ++i) this->fingers[i] = this->fingers[i + 1];
			--this->finger_count;
			this->ResetSecondary();
			if (this->finger_count == 1) {
				this->primary_down_time = SDL_GetTicks();
				this->primary_start_x = this->fingers[0].x;
				this->primary_start_y = this->fingers[0].y;
				this->primary_long_press = true; /* Do not trigger a new tap when lifting the last finger. */
			}
			return true;
		}

		if (this->primary_dragging) {
			this->ReleaseLeft();
		} else if (!this->primary_long_press) {
			this->MoveCursor(ev.tfinger.x, ev.tfinger.y);
			this->ClickLeft();
		}

		this->ResetPrimary();
		this->finger_count = 0;
		return true;
	}

	return false;
}
