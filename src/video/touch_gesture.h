/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/gpl-2.0>.
 */

/** @file touch_gesture.h Translation of touch input into OpenTTD mouse input. */

#ifndef TOUCH_GESTURE_H
#define TOUCH_GESTURE_H

#include <SDL.h>

/**
 * Translates SDL touch events into the mouse events OpenTTD understands.
 *
 * The mapping is intentionally conservative so that it works for every window
 * and widget without per-window code:
 *  - a short tap is a left click,
 *  - a long press is a right click,
 *  - a one-finger drag is a left-button drag (map panning uses the MapLMB
 *    scroll mode on Android; sliders and scrollbars keep working),
 *  - a two-finger gesture produces mouse wheel events (zoom on the map,
 *    scrolling in lists and scrollbars).
 */
class TouchGestureHandler {
public:
	/** Update the screen size used to convert normalised touch coordinates. */
	void SetScreenSize(int width, int height);

	/** Process pending long-press timeouts. Must be called every event-loop iteration. */
	void Tick();

	/**
	 * Handle an SDL touch event.
	 * @param ev The event to process.
	 * @return True if the event was consumed by the gesture handler.
	 */
	bool HandleEvent(const SDL_Event &ev);

private:
	struct Finger {
		SDL_FingerID id = 0;
		float x = 0.0f;
		float y = 0.0f;
	};

	static constexpr size_t MAX_FINGERS = 2;
	static constexpr Uint32 LONG_PRESS_MS = 500;

	Finger fingers[MAX_FINGERS];
	size_t finger_count = 0;

	int screen_width = 0;
	int screen_height = 0;

	bool primary_dragging = false;
	bool primary_long_press = false;
	bool two_finger_gesture = false;
	bool two_finger_zoom = false;
	Uint32 primary_down_time = 0;
	float primary_start_x = 0.0f;
	float primary_start_y = 0.0f;
	float two_finger_start_distance = 0.0f;
	float two_finger_last_distance = 0.0f;
	float two_finger_last_centroid_x = 0.0f;
	float two_finger_last_centroid_y = 0.0f;
	float two_finger_accumulated = 0.0f;

	Point TouchToScreen(float x, float y) const;
	void MoveCursor(float x, float y);
	void PressLeft();
	void ReleaseLeft();
	void ClickRight();
	void ClickLeft();
	void Wheel(int steps);
	void ResetPrimary();
	void ResetSecondary();
	void UpdateTwoFingerBaseline();
};

#endif /* TOUCH_GESTURE_H */
