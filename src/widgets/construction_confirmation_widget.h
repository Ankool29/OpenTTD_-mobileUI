/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation_widget.h Widgets of the construction confirmation bar. */

#ifndef WIDGETS_CONSTRUCTION_CONFIRMATION_WIDGET_H
#define WIDGETS_CONSTRUCTION_CONFIRMATION_WIDGET_H

/** Widgets of the #ConstructionConfirmationWindow class. */
enum ConstructionConfirmationWidgets : WidgetID {
	WID_CC_LABEL,  ///< Action description.
	WID_CC_CANCEL, ///< Cancel the pending proposal.
	WID_CC_CONFIRM, ///< Confirm the pending proposal.
};

#endif /* WIDGETS_CONSTRUCTION_CONFIRMATION_WIDGET_H */
