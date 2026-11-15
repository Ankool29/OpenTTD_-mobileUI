/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation_gui.cpp The non-modal construction confirmation bar. */

#include "stdafx.h"
#include "construction_confirmation.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"
#include "timer/timer.h"
#include "timer/timer_window.h"

#include <chrono>
#include "widgets/construction_confirmation_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** Window showing a pending construction proposal. */
struct ConstructionConfirmationWindow : Window {
	ConstructionConfirmationWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested();
		this->InvalidateData(0);
	}

	Point OnInitialPosition(int16_t sm_width, int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		int bottom = _screen.height;
		if (const Window *statusbar = FindWindowById(WindowClass::Statusbar, 0); statusbar != nullptr) bottom = statusbar->top;
		return {std::max<int>(0, (_screen.width - sm_width) / 2), std::max<int>(0, bottom - sm_height)};
	}

	StringID GetConfirmButtonString() const
	{
		const ConstructionProposal *proposal = ConstructionConfirmation().GetPending();
		if (proposal == nullptr) return STR_CONSTRUCTION_CONFIRM_BUILD;

		switch (proposal->action_type) {
			case ConstructionActionType::Build: return STR_CONSTRUCTION_CONFIRM_BUILD;
			case ConstructionActionType::Demolish: return STR_CONSTRUCTION_CONFIRM_DEMOLISH;
			case ConstructionActionType::Apply: return STR_CONSTRUCTION_CONFIRM_APPLY;
			default: NOT_REACHED();
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_CC_LABEL) return;

		const ConstructionProposal *proposal = ConstructionConfirmation().GetPending();
		if (proposal == nullptr) return;

		std::string label = proposal->label;
		if (proposal->estimated_cost.has_value()) {
			label = fmt::format("{} - {}", label, GetString(STR_JUST_CURRENCY_LONG, *proposal->estimated_cost));
		}

		DrawString(r.Shrink(WidgetDimensions::scaled.framerect), label, TextColour::White, AlignmentH::Start);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CC_CANCEL:
				ConstructionConfirmation().Cancel();
				break;

			case WID_CC_CONFIRM:
				ConstructionConfirmation().Confirm();
				break;

			default:
				break;
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->GetWidget<NWidgetCore>(WID_CC_CONFIRM)->SetString(this->GetConfirmButtonString());
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_construction_confirmation_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Grey, WID_CC_LABEL), SetMinimalSize(160, 30), SetFill(1, 1), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_CC_CANCEL), SetMinimalSize(70, 30), SetStringTip(STR_BUTTON_CANCEL, STR_BUTTON_CANCEL), SetFill(0, 1),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_CC_CONFIRM), SetMinimalSize(90, 30), SetStringTip(STR_CONSTRUCTION_CONFIRM_BUILD, STR_CONSTRUCTION_CONFIRM_BUILD), SetFill(0, 1),
	EndContainer(),
};

static WindowDesc _construction_confirmation_desc(
	WindowPosition::Manual, {}, 320, 45,
	WindowClass::ConstructionConfirmation, WindowClass::None,
	{WindowDefaultFlag::NoFocus, WindowDefaultFlag::NoClose},
	_nested_construction_confirmation_widgets
);

static bool _confirmation_show_pending = false;

/** Create or update the confirmation bar now. */
static void ShowConstructionConfirmationBar()
{
	Window *window = FindWindowById(WindowClass::ConstructionConfirmation, 0);
	if (window == nullptr) {
		window = new ConstructionConfirmationWindow(_construction_confirmation_desc);
	}

	window->InvalidateData(0);
	window->SetDirty();
	BringWindowToFrontById(WindowClass::ConstructionConfirmation, 0);
}

/** Presenter that creates, updates, and closes the confirmation bar. */
class ConstructionConfirmationGuiPresenter : public ConstructionConfirmationPresenter {
public:
	void Show(const ConstructionProposal &) override
	{
		/* The demo can be selected while the toolbar dropdown is still being torn
		 * down. Creating another window from that callback can leave the new
		 * window behind the dropdown or without a first paint. Defer until the
		 * dropdown is gone and the current GUI event has completed. */
		if (FindWindowById(WindowClass::DropdownMenu, 0) != nullptr) {
			_confirmation_show_pending = true;
			return;
		}
		ShowConstructionConfirmationBar();
	}

	void Hide() override
	{
		_confirmation_show_pending = false;
		CloseWindowById(WindowClass::ConstructionConfirmation, 0);
	}
};

static ConstructionConfirmationGuiPresenter _construction_confirmation_presenter;

static const IntervalTimer<TimerWindow> _confirmation_show_timer = {std::chrono::milliseconds(1), [](auto) {
	if (!_confirmation_show_pending) return;
	if (FindWindowById(WindowClass::DropdownMenu, 0) != nullptr) return;

	_confirmation_show_pending = false;
	if (ConstructionConfirmation().IsPending()) ShowConstructionConfirmationBar();
}};

/** Attach the confirmation bar presenter to the controller. */
void InitConstructionConfirmationGui()
{
	ConstructionConfirmation().SetPresenter(&_construction_confirmation_presenter);
}
