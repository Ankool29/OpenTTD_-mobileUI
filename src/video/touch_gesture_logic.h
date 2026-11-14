/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file touch_gesture_logic.h Platform-independent touch gesture state machine. */

#ifndef TOUCH_GESTURE_LOGIC_H
#define TOUCH_GESTURE_LOGIC_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

/** State of the touch gesture recogniser. */
enum class TouchGesturePhase : uint8_t {
	Idle,             ///< No finger is being tracked.
	PrimaryDown,      ///< One finger is down; tap versus drag is not decided yet.
	PrimaryDragging,  ///< One finger is dragging and the left mouse button is held.
	TwoFinger,        ///< Two fingers are down; no mouse button is held.
};

/** Actions the gesture recogniser wants the platform layer to perform. */
struct TouchGestureActions {
	bool move_cursor = false; ///< Move the mouse cursor to the given normalised position.
	float cursor_x = 0.0f;    ///< Normalised cursor X position.
	float cursor_y = 0.0f;    ///< Normalised cursor Y position.
	bool reset_delta = false; ///< Reset the accumulated mouse delta.
	bool press_left = false;  ///< Press the left mouse button.
	bool release_left = false;///< Release the left mouse button.
	bool click_left = false;  ///< Perform a complete left click.
	bool click_right = false; ///< Perform a complete right click.
	bool cancel_placement = false; ///< Cancel a sizing drag without calling OnPlaceMouseUp.
	bool pan = false;         ///< Scroll the viewport by the given normalised delta.
	float pan_dx = 0.0f;      ///< Normalised horizontal viewport scroll delta.
	float pan_dy = 0.0f;      ///< Normalised vertical viewport scroll delta.
	float pan_x = 0.0f;       ///< Normalised centroid X used to find the viewport.
	float pan_y = 0.0f;       ///< Normalised centroid Y used to find the viewport.
	int wheel_steps = 0;      ///< Mouse wheel steps; positive is scroll down.
	bool update_mouse = false;///< Run a mouse update after moving the cursor.
};

/**
 * Platform-independent touch gesture state machine.
 *
 * The class deliberately does not depend on SDL or OpenTTD mouse state, so its
 * transitions can be tested without a video driver.
 */
class TouchGestureLogic {
public:
	static constexpr size_t MAX_FINGERS = 2;
	static constexpr uint32_t LONG_PRESS_MS = 500;

	/** Update the screen size used to convert normalised coordinates. */
	void SetScreenSize(int width, int height);

	/** Set the movement in pixels after which a one-finger touch becomes a drag. */
	void SetDragThreshold(float threshold) { this->drag_threshold = threshold; }

	int GetScreenWidth() const { return this->screen_width; }
	int GetScreenHeight() const { return this->screen_height; }
	TouchGesturePhase GetPhase() const { return this->phase; }
	size_t GetFingerCount() const { return this->finger_count; }
	bool IsPrimaryLongPress() const { return this->primary_long_press; }
	bool IsTwoFingerZoom() const { return this->two_finger_zoom; }

	/** Convert a normalised SDL position to screen pixels. */
	std::pair<int, int> TouchToScreen(float x, float y) const;

	TouchGestureActions HandleDown(uint64_t id, float x, float y, uint32_t now);
	TouchGestureActions HandleMotion(uint64_t id, float x, float y);
	TouchGestureActions HandleUp(uint64_t id, float x, float y, uint32_t now);
	TouchGestureActions Tick(uint32_t now);

	/** Cancel any active gesture and release a touch-owned left button. */
	TouchGestureActions Reset();

private:
	struct Finger {
		uint64_t id = 0;
		float x = 0.0f;
		float y = 0.0f;
	};

	std::array<Finger, MAX_FINGERS> fingers{};
	size_t finger_count = 0;

	int screen_width = 0;
	int screen_height = 0;
	float drag_threshold = 10.0f;

	TouchGesturePhase phase = TouchGesturePhase::Idle;
	bool primary_long_press = false;
	bool two_finger_zoom = false;
	uint32_t primary_down_time = 0;
	float primary_start_x = 0.0f;
	float primary_start_y = 0.0f;

	float two_finger_start_distance = 0.0f;
	float two_finger_start_centroid_x = 0.0f;
	float two_finger_start_centroid_y = 0.0f;
	float two_finger_last_distance = 0.0f;
	float two_finger_last_centroid_x = 0.0f;
	float two_finger_last_centroid_y = 0.0f;
	float two_finger_accumulated = 0.0f;
	float two_finger_pan_accum_x = 0.0f;
	float two_finger_pan_accum_y = 0.0f;
	bool two_finger_mode_decided = false;

	size_t FindFinger(uint64_t id) const;
	void UpdateTwoFingerBaseline();
};

#endif /* TOUCH_GESTURE_LOGIC_H */
