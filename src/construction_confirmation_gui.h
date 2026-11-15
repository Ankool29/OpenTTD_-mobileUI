/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation_gui.h Testable helpers for the confirmation bar. */

#ifndef CONSTRUCTION_CONFIRMATION_GUI_H
#define CONSTRUCTION_CONFIRMATION_GUI_H

#include "gfx_type.h"

#ifdef TOUCH_DIAGNOSTIC
extern bool _construction_confirmation_center_marker; ///< Draw a diagnostic marker in the main window.
#endif

/**
 * Compute the bottom-centre position of the confirmation bar.
 * @param screen_width Screen width.
 * @param screen_height Screen height.
 * @param statusbar_top Top of the statusbar, or a negative value when absent.
 * @param width Bar width.
 * @param height Bar height.
 * @return Top-left position, clamped to the screen.
 */
Point GetConstructionConfirmationBarPosition(int screen_width, int screen_height, int statusbar_top, int width, int height);

/** Small state machine for deferring presentation until a dropdown is gone. */
class ConstructionConfirmationPresentationQueue {
public:
	/** Request presentation. Returns true when it can be shown immediately. */
	bool Request(bool dropdown_open)
	{
		if (dropdown_open) {
			this->pending = true;
			return false;
		}
		this->pending = false;
		return true;
	}

	/** Process a GUI tick. Returns true when a pending presentation should run now. */
	bool Tick(bool dropdown_open, bool proposal_pending)
	{
		if (!this->pending) return false;
		if (dropdown_open) return false;
		this->pending = false;
		return proposal_pending;
	}

	/** Discard a deferred presentation. */
	void Cancel() { this->pending = false; }

	/** Is presentation still waiting for a dropdown to close? */
	bool IsPending() const { return this->pending; }

private:
	bool pending = false;
};

#endif /* CONSTRUCTION_CONFIRMATION_GUI_H */
