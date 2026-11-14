/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file touch_gesture.h Translation of touch input into OpenTTD mouse input. */

#ifndef TOUCH_GESTURE_H
#define TOUCH_GESTURE_H

#include <SDL.h>
#include "touch_gesture_logic.h"

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

	/** Cancel an interrupted gesture without leaving a mouse button pressed. */
	void Cancel();

private:
	TouchGestureLogic logic;

	TouchGesturePhase last_phase = TouchGesturePhase::Idle;
	size_t last_finger_count = 0;
	bool last_long_press = false;
	bool last_two_finger_zoom = false;

	void ApplyActions(const TouchGestureActions &actions);
	void LogStateChange(const char *event);

	void MoveCursor(float x, float y);
	void PressLeft();
	void ReleaseLeft();
	void ClickLeft();
	void ClickRight();
	void Wheel(int steps);
};

#endif /* TOUCH_GESTURE_H */
