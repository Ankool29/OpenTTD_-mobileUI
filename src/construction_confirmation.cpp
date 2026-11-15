/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation.cpp GUI-side confirmation of human construction actions. */

#include "stdafx.h"
#include "construction_confirmation.h"
#include "construction_confirmation_diagnostic.h"
#include "company_func.h"
#include "map_func.h"
#include "window_func.h"
#include "window_gui.h"

#include "safeguards.h"

ConstructionConfirmationController::ConstructionConfirmationController(ConstructionConfirmationPresenter *presenter) : presenter(presenter)
{
}

void ConstructionConfirmationController::SetPresenter(ConstructionConfirmationPresenter *new_presenter)
{
	ConstructionDiag("presenter set: old={} new={} pending={}", fmt::ptr(this->presenter), fmt::ptr(new_presenter), this->pending.has_value());
	if (this->presenter == new_presenter) return;
	if (this->presenter != nullptr && this->pending.has_value()) this->presenter->Hide();
	this->presenter = new_presenter;
	if (this->presenter != nullptr && this->pending.has_value()) this->presenter->Show(*this->pending);
}

bool ConstructionConfirmationController::Submit(ConstructionProposal proposal)
{
	if (!proposal.execute) {
		ConstructionDiag("submit rejected: empty execute callback");
		return false;
	}

	proposal.token = this->next_token++;
	proposal.company = _local_company;
	proposal.map_context = {Map::SizeX(), Map::SizeY(), this->map_epoch};
	ConstructionDiag("submit token={} action={} area=({},{}) owner={} company={} map={}x{} epoch={} label='{}'",
			proposal.token, static_cast<int>(proposal.action_type), proposal.area.tile.base(), proposal.area.w, proposal.area.h,
			proposal.owner.has_value(), proposal.company.base(), proposal.map_context.width, proposal.map_context.height,
			proposal.map_context.epoch, proposal.label);
	this->pending = std::move(proposal);

	if (this->presenter != nullptr) this->presenter->Show(*this->pending);
	return true;
}

bool ConstructionConfirmationController::Confirm()
{
	if (!this->pending.has_value()) return false;
	return this->Confirm(this->pending->token);
}

bool ConstructionConfirmationController::Confirm(uint64_t token)
{
	if (!this->pending.has_value()) {
		ConstructionDiag("confirm token={} rejected: no pending proposal", token);
		return false;
	}
	if (this->pending->token != token) {
		ConstructionDiag("confirm token={} rejected: current token={}", token, this->pending->token);
		return false;
	}
	if (!this->IsValid(*this->pending)) {
		ConstructionDiag("confirm token={} rejected: stale owner/company/map context", token);
		this->Clear("invalid context");
		return false;
	}

	if (this->pending->validate) {
		const ConstructionValidationResult result = this->pending->validate();
		ConstructionDiag("confirm token={} validation={}", token, static_cast<int>(result));
		if (result == ConstructionValidationResult::Invalid) {
			this->Clear("validation rejected");
			return false;
		}
	}

	ConstructionProposal proposal = std::move(*this->pending);
	this->pending.reset();
	ConstructionDiag("confirm token={} consumed, hiding bar and executing", token);
	this->HidePresenter();
	proposal.execute();
	ConstructionDiag("confirm token={} execute callback returned", token);
	return true;
}

void ConstructionConfirmationController::Cancel()
{
	ConstructionDiag("cancel requested pending={}", this->pending.has_value());
	if (!this->pending.has_value()) return;
	this->Clear("cancel");
}

void ConstructionConfirmationController::Invalidate(std::string_view reason)
{
	ConstructionDiag("invalidate reason='{}' pending={}", reason, this->pending.has_value());
	if (!this->pending.has_value()) return;
	this->Clear(reason);
}

void ConstructionConfirmationController::InvalidateForWindow(WindowClass window_class, WindowNumber window_number, uint64_t identity, std::string_view reason)
{
	if (!this->pending.has_value() || !this->pending->owner.has_value()) return;
	const ConstructionWindowIdentity &owner = *this->pending->owner;
	ConstructionDiag("window close class={} number={} identity={} owner={}/{}/{} reason='{}'",
			static_cast<int>(window_class), static_cast<int32_t>(window_number), identity,
			static_cast<int>(owner.window_class), static_cast<int32_t>(owner.window_number), owner.identity, reason);
	if (owner.window_class == window_class && owner.window_number == window_number && owner.identity == identity) {
		this->Clear(reason);
	}
}

void ConstructionConfirmationController::NotifyCompanyChanged()
{
	ConstructionDiag("company changed");
	this->Invalidate("company changed");
}

void ConstructionConfirmationController::NotifyMapChanged()
{
	this->map_epoch++;
	ConstructionDiag("map changed epoch={}", this->map_epoch);
	this->Invalidate("map changed");
}

bool ConstructionConfirmationController::IsValid(const ConstructionProposal &proposal) const
{
	if (proposal.company != _local_company) return false;
	if (proposal.map_context.epoch != this->map_epoch) return false;
	if (proposal.map_context.width != Map::SizeX() || proposal.map_context.height != Map::SizeY()) return false;

	if (proposal.owner.has_value()) {
		const ConstructionWindowIdentity &owner = *proposal.owner;
		const Window *window = FindWindowById(owner.window_class, owner.window_number);
		if (window == nullptr || window->GetIdentity() != owner.identity) return false;
	}

	return true;
}

void ConstructionConfirmationController::Clear(std::string_view reason)
{
	ConstructionDiag("clear reason='{}' had_pending={} token={}", reason, this->pending.has_value(), this->pending.has_value() ? this->pending->token : 0);
	this->pending.reset();
	this->HidePresenter();
}

void ConstructionConfirmationController::HidePresenter()
{
	ConstructionDiag("hide presenter ptr={}", fmt::ptr(this->presenter));
	if (this->presenter != nullptr) this->presenter->Hide();
}

ConstructionConfirmationController &ConstructionConfirmation()
{
	static ConstructionConfirmationController controller;
	return controller;
}

void ShowConstructionConfirmationDemo()
{
	ConstructionDiag("demo requested");
	ConstructionProposal proposal;
	proposal.action_type = ConstructionActionType::Apply;
	proposal.label = "Demo construction confirmation (no command)";
	proposal.execute = [] {};
	ConstructionConfirmation().Submit(std::move(proposal));
}
