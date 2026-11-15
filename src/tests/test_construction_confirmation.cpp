/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file test_construction_confirmation.cpp Tests for the GUI construction confirmation foundation. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../construction_confirmation.h"
#include "../map_func.h"
#include "../tilehighlight_func.h"
#include "../viewport_func.h"
#include "../video/touch_gesture_logic.h"

#include "../safeguards.h"

struct FakeConstructionConfirmationPresenter : ConstructionConfirmationPresenter {
	int shown = 0;
	int hidden = 0;

	void Show(const ConstructionProposal &) override { this->shown++; }
	void Hide() override { this->hidden++; }
};

static ConstructionProposal MakeProposal(int &calls, const char *label = "Test construction")
{
	ConstructionProposal proposal;
	proposal.action_type = ConstructionActionType::Build;
	proposal.label = label;
	proposal.area = TileArea(TileIndex{0}, 1, 1);
	proposal.execute = [&calls]() { calls++; };
	return proposal;
}

TEST_CASE("ConstructionConfirmation - submit does not execute")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	CHECK(controller.Submit(MakeProposal(calls)));
	CHECK(controller.IsPending());
	CHECK(calls == 0);
	CHECK(presenter.shown == 1);
}

TEST_CASE("ConstructionConfirmation - confirm executes exactly once")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	const uint64_t token = controller.GetPendingToken();
	CHECK(controller.Confirm(token));
	CHECK(calls == 1);
	CHECK_FALSE(controller.IsPending());
	CHECK_FALSE(controller.Confirm(token));
	CHECK(calls == 1);
	CHECK(presenter.hidden == 1);
}

TEST_CASE("ConstructionConfirmation - double confirm does not execute twice")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	CHECK(controller.Confirm());
	CHECK_FALSE(controller.Confirm());
	CHECK(calls == 1);
}

TEST_CASE("ConstructionConfirmation - cancel executes nothing")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	_thd.place_mode = HT_RECT;
	REQUIRE(controller.Submit(MakeProposal(calls)));
	controller.Cancel();
	CHECK_FALSE(controller.IsPending());
	CHECK(calls == 0);
	CHECK(_thd.place_mode == HT_RECT);
	CHECK(presenter.hidden == 1);
}

TEST_CASE("ConstructionConfirmation - invalidate executes nothing")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	controller.Invalidate();
	CHECK_FALSE(controller.IsPending());
	CHECK(calls == 0);
	CHECK(presenter.hidden == 1);
}

TEST_CASE("ConstructionConfirmation - proposal replaces the previous one")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int first_calls = 0;
	int second_calls = 0;

	REQUIRE(controller.Submit(MakeProposal(first_calls, "First")));
	const uint64_t first_token = controller.GetPendingToken();
	REQUIRE(controller.Submit(MakeProposal(second_calls, "Second")));
	const uint64_t second_token = controller.GetPendingToken();

	CHECK(first_token != second_token);
	CHECK_FALSE(controller.Confirm(first_token));
	CHECK(first_calls == 0);
	CHECK(second_calls == 0);
	CHECK(controller.IsPending());
	CHECK(controller.Confirm(second_token));
	CHECK(first_calls == 0);
	CHECK(second_calls == 1);
}

TEST_CASE("ConstructionConfirmation - presenter can be attached after submit")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller;
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	CHECK(controller.IsPending());
	CHECK(presenter.shown == 0);

	controller.SetPresenter(&presenter);
	CHECK(presenter.shown == 1);
	CHECK(controller.IsPending());
	controller.Cancel();
	CHECK(presenter.hidden == 1);
	CHECK(calls == 0);
}

TEST_CASE("ConstructionConfirmation - presenter reattachment preserves the proposal")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter first;
	FakeConstructionConfirmationPresenter second;
	ConstructionConfirmationController controller(&first);
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	controller.SetPresenter(&second);
	CHECK(first.hidden == 1);
	CHECK(second.shown == 1);
	CHECK(controller.IsPending());
	CHECK(controller.Confirm());
	CHECK(calls == 1);
}

TEST_CASE("ConstructionConfirmation - validation can reject a proposal")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	ConstructionProposal proposal = MakeProposal(calls);
	proposal.validate = []() { return ConstructionValidationResult::Invalid; };
	REQUIRE(controller.Submit(std::move(proposal)));
	CHECK_FALSE(controller.Confirm());
	CHECK(calls == 0);
	CHECK_FALSE(controller.IsPending());
}

TEST_CASE("ConstructionConfirmation - company and map changes invalidate")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	controller.NotifyCompanyChanged();
	CHECK_FALSE(controller.IsPending());
	CHECK(calls == 0);

	REQUIRE(controller.Submit(MakeProposal(calls)));
	controller.NotifyMapChanged();
	CHECK_FALSE(controller.IsPending());
	CHECK(calls == 0);
}

TEST_CASE("ConstructionConfirmation - owner window close invalidates by identity")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	ConstructionProposal proposal = MakeProposal(calls);
	proposal.owner = ConstructionWindowIdentity{WindowClass::Company, 0, 42};
	REQUIRE(controller.Submit(std::move(proposal)));

	controller.InvalidateForWindow(WindowClass::Company, 0, 43);
	CHECK(controller.IsPending());

	controller.InvalidateForWindow(WindowClass::Company, 0, 42);
	CHECK_FALSE(controller.IsPending());
	CHECK(calls == 0);
}

TEST_CASE("ConstructionConfirmation - tool parameter change invalidates")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController &controller = ConstructionConfirmation();
	controller.SetPresenter(&presenter);
	controller.Cancel();
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	SetTileSelectSize(1, 1);
	CHECK_FALSE(controller.IsPending());
	CHECK(calls == 0);

	controller.SetPresenter(nullptr);
}

TEST_CASE("ConstructionConfirmation - focus loss invalidates")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));
	controller.Invalidate();
	CHECK_FALSE(controller.IsPending());
	CHECK(calls == 0);
}

TEST_CASE("ConstructionConfirmation - rotation and two-finger pan keep the proposal")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController controller(&presenter);
	int calls = 0;

	REQUIRE(controller.Submit(MakeProposal(calls)));

	TouchGestureLogic logic;
	logic.SetScreenSize(1000, 1000);
	logic.HandleDown(1, 0.40f, 0.60f, 0);
	logic.HandleDown(2, 0.60f, 0.60f, 10);
	logic.HandleMotion(1, 0.45f, 0.65f);
	const TouchGestureActions pan = logic.HandleMotion(2, 0.65f, 0.65f);
	CHECK(pan.pan);
	CHECK(controller.IsPending());

	logic.SetScreenSize(2000, 500);
	CHECK(controller.IsPending());
	CHECK(controller.Confirm());
	CHECK(calls == 1);
}

TEST_CASE("ConstructionConfirmation - demo never changes the map")
{
	Map::Allocate(64, 64);
	FakeConstructionConfirmationPresenter presenter;
	ConstructionConfirmationController &controller = ConstructionConfirmation();
	controller.SetPresenter(&presenter);
	controller.Cancel();

	ShowConstructionConfirmationDemo();
	REQUIRE(controller.IsPending());
	const uint32_t map_size = Map::Size();
	CHECK(controller.Confirm());
	CHECK(Map::Size() == map_size);
	CHECK_FALSE(controller.IsPending());

	controller.SetPresenter(nullptr);
}
