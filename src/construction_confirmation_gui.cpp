/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation_gui.cpp The non-modal construction confirmation bar. */

#include "stdafx.h"
#include "construction_confirmation.h"
#include "construction_confirmation_diagnostic.h"
#include "construction_confirmation_gui.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"
#include "timer/timer.h"
#include "timer/timer_window.h"

#include <chrono>
#include <algorithm>
#include "widgets/construction_confirmation_widget.h"

#include "table/strings.h"

#include "safeguards.h"

Point GetConstructionConfirmationBarPosition(int screen_width, int screen_height, int statusbar_top, int width, int height)
{
	const int bottom = statusbar_top >= 0 ? statusbar_top : screen_height;
	return {std::max(0, (screen_width - width) / 2), std::max(0, bottom - height)};
}

/** Window showing a pending construction proposal. */
struct ConstructionConfirmationWindow : Window {
#ifdef TOUCH_DIAGNOSTIC
	mutable bool diagnostic_paint_logged = false;
#endif

	ConstructionConfirmationWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested();
		ConstructionDiag("window constructed: class={} number={} pos=({},{}) size={}x{} z={} nested={}x{}",
				static_cast<int>(this->window_class), static_cast<int32_t>(this->window_number), this->left, this->top, this->width, this->height,
				this->z_position != _z_windows.end() && *this->z_position == this, this->nested_root->smallest_x, this->nested_root->smallest_y);
		this->InvalidateData(0);
	}

	Point OnInitialPosition(int16_t sm_width, int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		const Window *statusbar = FindWindowById(WindowClass::Statusbar, 0);
		const int statusbar_top = statusbar != nullptr ? statusbar->top : -1;
		const Point pt = GetConstructionConfirmationBarPosition(_screen.width, _screen.height, statusbar_top, sm_width, sm_height);
		ConstructionDiag("initial position: screen={}x{} statusbar_top={} sm={}x{} -> ({},{})",
				_screen.width, _screen.height, statusbar_top, sm_width, sm_height, pt.x, pt.y);
		return pt;
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
#ifdef TOUCH_DIAGNOSTIC
		if (!this->diagnostic_paint_logged) {
			this->diagnostic_paint_logged = true;
			ConstructionDiag("first paint: pending={} token={} widget_rect=({},{},{},{}) window=({},{}) {}x{}",
					proposal != nullptr, proposal != nullptr ? proposal->token : 0, r.left, r.top, r.right, r.bottom,
					this->left, this->top, this->width, this->height);
		}
#endif
		if (proposal == nullptr) return;

		std::string label = proposal->label;
		if (proposal->estimated_cost.has_value()) {
			label = fmt::format("{} - {}", label, GetString(STR_JUST_CURRENCY_LONG, *proposal->estimated_cost));
		}

		DrawString(r.Shrink(WidgetDimensions::scaled.framerect), label, TextColour::White, AlignmentH::Start);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		ConstructionDiag("bar click widget={} window=({},{}) {}x{}", widget, this->left, this->top, this->width, this->height);
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
		ConstructionDiag("bar invalidate/paint request: pos=({},{}) size={}x{} z={}",
				this->left, this->top, this->width, this->height, this->z_position != _z_windows.end() && *this->z_position == this);
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

static ConstructionConfirmationPresentationQueue _confirmation_presentation;

/** Log the visible state of the confirmation bar. */
static void LogConstructionConfirmationBarState(const char *step)
{
	Window *window = FindWindowById(WindowClass::ConstructionConfirmation, 0);
	if (window == nullptr) {
		ConstructionDiag("{}: window absent", step);
		return;
	}

	const bool z_visible = window->z_position != _z_windows.end() && *window->z_position == window;
	ConstructionDiag("{}: pos=({},{}) size={}x{} z_visible={} collapsed={} shaded={}",
			step, window->left, window->top, window->width, window->height, z_visible, window->IsCollapsed(), window->IsShaded());
}

/** Create or update the confirmation bar now. */
static void ShowConstructionConfirmationBar()
{
	ConstructionDiag("show bar: creating/updating");
	Window *window = FindWindowById(WindowClass::ConstructionConfirmation, 0);
	if (window == nullptr) {
		window = new ConstructionConfirmationWindow(_construction_confirmation_desc);
		ConstructionDiag("show bar: window created ptr={}", fmt::ptr(window));
	}

	window->InvalidateData(0);
	window->SetDirty();
	BringWindowToFrontById(WindowClass::ConstructionConfirmation, 0);
	LogConstructionConfirmationBarState("show bar after foreground");
}

/** Presenter that creates, updates, and closes the confirmation bar. */
class ConstructionConfirmationGuiPresenter : public ConstructionConfirmationPresenter {
public:
	void Show(const ConstructionProposal &proposal) override
	{
		const bool dropdown_open = FindWindowById(WindowClass::DropdownMenu, 0) != nullptr;
		ConstructionDiag("presenter show: token={} dropdown_open={} pending={}",
				proposal.token, dropdown_open, ConstructionConfirmation().IsPending());
		if (!_confirmation_presentation.Request(dropdown_open)) {
			ConstructionDiag("presenter show: deferred until dropdown closes");
			return;
		}
		ShowConstructionConfirmationBar();
	}

	void Hide() override
	{
		ConstructionDiag("presenter hide: deferred={}", _confirmation_presentation.IsPending());
		_confirmation_presentation.Cancel();
		CloseWindowById(WindowClass::ConstructionConfirmation, 0);
		LogConstructionConfirmationBarState("presenter hide after close");
	}
};

static ConstructionConfirmationGuiPresenter _construction_confirmation_presenter;

static const IntervalTimer<TimerWindow> _confirmation_show_timer = {std::chrono::milliseconds(1), [](auto) {
	if (!_confirmation_presentation.IsPending()) return;
	const bool dropdown_open = FindWindowById(WindowClass::DropdownMenu, 0) != nullptr;
	const bool proposal_pending = ConstructionConfirmation().IsPending();
	const bool show = _confirmation_presentation.Tick(dropdown_open, proposal_pending);
	ConstructionDiag("deferred timer tick: dropdown_open={} proposal_pending={} show={}", dropdown_open, proposal_pending, show);
	if (show) ShowConstructionConfirmationBar();
}};

/** Attach the confirmation bar presenter to the controller. */
void InitConstructionConfirmationGui()
{
	ConstructionDiag("presenter init");
	ConstructionConfirmation().SetPresenter(&_construction_confirmation_presenter);
}
