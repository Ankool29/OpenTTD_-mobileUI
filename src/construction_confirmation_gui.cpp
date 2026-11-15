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
#include "blitter/factory.hpp"
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

#ifdef TOUCH_DIAGNOSTIC
bool _construction_confirmation_center_marker = false;
#endif

Point GetConstructionConfirmationBarPosition(int screen_width, int screen_height, int statusbar_top, int width, int height)
{
	const int bottom = statusbar_top >= 0 ? statusbar_top : screen_height;
	return {std::max(0, (screen_width - width) / 2), std::max(0, bottom - height)};
}

#ifdef TOUCH_DIAGNOSTIC
/** Read one framebuffer pixel at absolute screen coordinates. */
static uint32_t SampleConfirmScreenPixel(int x, int y)
{
	if (_screen.dst_ptr == nullptr || x < 0 || y < 0 || x >= _screen.width || y >= _screen.height) return 0xDEADBEEF;
	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	uint32_t pixel = 0;
	blitter->CopyImageToBuffer(blitter->MoveTo(_screen.dst_ptr, x, y), &pixel, 1, 1, 1);
	return pixel;
}
#endif

/** Window showing a pending construction proposal. */
struct ConstructionConfirmationWindow : Window {
#ifdef TOUCH_DIAGNOSTIC
	mutable bool diagnostic_paint_logged = false;
	mutable int diagnostic_paint_count = 0;
#endif

	ConstructionConfirmationWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested();
#ifdef TOUCH_DIAGNOSTIC
		this->GetWidget<NWidgetCore>(WID_CC_LABEL)->colour = Colours::Red;
		this->GetWidget<NWidgetCore>(WID_CC_CANCEL)->colour = Colours::Yellow;
		this->GetWidget<NWidgetCore>(WID_CC_CONFIRM)->colour = Colours::Yellow;
#endif
		ConstructionDiag("window constructed: class={} number={} pos=({},{}) size={}x{} z={} nested={}x{} gui_scale={} gui_zoom={}",
				static_cast<int>(this->window_class), static_cast<int32_t>(this->window_number), this->left, this->top, this->width, this->height,
				this->z_position != _z_windows.end() && *this->z_position == this, this->nested_root->smallest_x, this->nested_root->smallest_y,
				_gui_scale, _gui_zoom);
		ConstructionDiag("widget geometry: label=({},{}) {}x{} cancel=({},{}) {}x{} confirm=({},{}) {}x{}",
				this->GetWidget<NWidgetBase>(WID_CC_LABEL)->pos_x, this->GetWidget<NWidgetBase>(WID_CC_LABEL)->pos_y,
				this->GetWidget<NWidgetBase>(WID_CC_LABEL)->current_x, this->GetWidget<NWidgetBase>(WID_CC_LABEL)->current_y,
				this->GetWidget<NWidgetBase>(WID_CC_CANCEL)->pos_x, this->GetWidget<NWidgetBase>(WID_CC_CANCEL)->pos_y,
				this->GetWidget<NWidgetBase>(WID_CC_CANCEL)->current_x, this->GetWidget<NWidgetBase>(WID_CC_CANCEL)->current_y,
				this->GetWidget<NWidgetBase>(WID_CC_CONFIRM)->pos_x, this->GetWidget<NWidgetBase>(WID_CC_CONFIRM)->pos_y,
				this->GetWidget<NWidgetBase>(WID_CC_CONFIRM)->current_x, this->GetWidget<NWidgetBase>(WID_CC_CONFIRM)->current_y);
		this->InvalidateData(0);
	}

	Point OnInitialPosition(int16_t sm_width, int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		const Window *statusbar = FindWindowById(WindowClass::Statusbar, 0);
		const int statusbar_top = statusbar != nullptr ? statusbar->top : -1;
		const Point pt = GetConstructionConfirmationBarPosition(_screen.width, _screen.height, statusbar_top, sm_width, sm_height);
		ConstructionDiag("initial position: screen={}x{} statusbar=({},{}) {}x{} toolbar=({},{}) {}x{} sm={}x{} -> ({},{})",
				_screen.width, _screen.height,
				statusbar != nullptr ? statusbar->left : -1, statusbar != nullptr ? statusbar->top : -1,
				statusbar != nullptr ? statusbar->width : 0, statusbar != nullptr ? statusbar->height : 0,
				FindWindowById(WindowClass::MainToolbar, 0) != nullptr ? FindWindowById(WindowClass::MainToolbar, 0)->left : -1,
				FindWindowById(WindowClass::MainToolbar, 0) != nullptr ? FindWindowById(WindowClass::MainToolbar, 0)->top : -1,
				FindWindowById(WindowClass::MainToolbar, 0) != nullptr ? FindWindowById(WindowClass::MainToolbar, 0)->width : 0,
				FindWindowById(WindowClass::MainToolbar, 0) != nullptr ? FindWindowById(WindowClass::MainToolbar, 0)->height : 0,
				sm_width, sm_height, pt.x, pt.y);
		return pt;
	}

#ifdef TOUCH_DIAGNOSTIC
	void OnPaint() override
	{
		const DrawPixelInfo *dpi = _cur_dpi;
		const int sample_x1 = this->left + 10;
		const int sample_y1 = this->top + 10;
		const int sample_x2 = this->left + this->width / 2;
		const int sample_y2 = this->top + this->height / 2;
		const int sample_x3 = this->left + this->width - 30;
		const int sample_y3 = this->top + this->height / 2;
		const uint32_t before1 = SampleConfirmScreenPixel(sample_x1, sample_y1);
		const uint32_t before2 = SampleConfirmScreenPixel(sample_x2, sample_y2);
		const uint32_t before3 = SampleConfirmScreenPixel(sample_x3, sample_y3);
		ConstructionDiag("OnPaint[{}]: window=({},{}) {}x{} dpi=({},{}) {}x{} samples_before={:08X},{:08X},{:08X}",
				this->diagnostic_paint_count, this->left, this->top, this->width, this->height,
				dpi != nullptr ? dpi->left : -1, dpi != nullptr ? dpi->top : -1,
				dpi != nullptr ? dpi->width : 0, dpi != nullptr ? dpi->height : 0,
				before1, before2, before3);
		this->Window::OnPaint();
		const uint32_t after1 = SampleConfirmScreenPixel(sample_x1, sample_y1);
		const uint32_t after2 = SampleConfirmScreenPixel(sample_x2, sample_y2);
		const uint32_t after3 = SampleConfirmScreenPixel(sample_x3, sample_y3);
		if (this->diagnostic_paint_count < 8) {
			ConstructionDiag("OnPaint[{}] samples_after={:08X},{:08X},{:08X} changed={},{},{}",
					this->diagnostic_paint_count, after1, after2, after3,
					before1 != after1, before2 != after2, before3 != after3);
		}
		++this->diagnostic_paint_count;
	}
#endif

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
	const Window *statusbar = FindWindowById(WindowClass::Statusbar, 0);
	const Window *toolbar = FindWindowById(WindowClass::MainToolbar, 0);
	ConstructionDiag("{}: screen={}x{} statusbar=({},{}) {}x{} toolbar=({},{}) {}x{}",
			step, _screen.width, _screen.height,
			statusbar != nullptr ? statusbar->left : -1, statusbar != nullptr ? statusbar->top : -1,
			statusbar != nullptr ? statusbar->width : 0, statusbar != nullptr ? statusbar->height : 0,
			toolbar != nullptr ? toolbar->left : -1, toolbar != nullptr ? toolbar->top : -1,
			toolbar != nullptr ? toolbar->width : 0, toolbar != nullptr ? toolbar->height : 0);
	if (window == nullptr) {
		ConstructionDiag("{}: window absent", step);
		return;
	}

	const bool z_visible = window->z_position != _z_windows.end() && *window->z_position == window;
	ConstructionDiag("{}: pos=({},{}) size={}x{} z_visible={} collapsed={} shaded={}",
			step, window->left, window->top, window->width, window->height, z_visible, window->IsCollapsed(), window->IsShaded());

	std::string above;
	int count = 0;
	for (Window *other : Window::IterateFromFront()) {
		if (other == window) break;
		if (count++ >= 8) break;
		if (!above.empty()) above += ", ";
		above += fmt::format("{}/{}@({},{}) {}x{}", static_cast<int>(other->window_class), static_cast<int32_t>(other->window_number),
				other->left, other->top, other->width, other->height);
	}
	ConstructionDiag("{}: windows in front={}", step, above.empty() ? "none" : above);
}

/** Create or update the confirmation bar now. */
static void ShowConstructionConfirmationBar()
{
#ifdef TOUCH_DIAGNOSTIC
	_construction_confirmation_center_marker = true;
	MarkWholeScreenDirty();
	ConstructionDiag("center marker enabled");
#endif
	ConstructionDiag("show bar: creating/updating");
	Window *window = FindWindowById(WindowClass::ConstructionConfirmation, 0);
	if (window == nullptr) {
#ifdef TOUCH_DIAGNOSTIC
		try {
			window = new ConstructionConfirmationWindow(_construction_confirmation_desc);
		} catch (const std::exception &e) {
			ConstructionDiag("show bar: window creation exception: {}", e.what());
			return;
		}
#else
		window = new ConstructionConfirmationWindow(_construction_confirmation_desc);
#endif
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
#ifdef TOUCH_DIAGNOSTIC
		_construction_confirmation_center_marker = false;
		MarkWholeScreenDirty();
		ConstructionDiag("center marker disabled");
#endif
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
