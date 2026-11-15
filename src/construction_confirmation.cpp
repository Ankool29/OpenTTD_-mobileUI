/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation.cpp GUI-side confirmation of human construction actions. */

#include "stdafx.h"
#include "construction_confirmation.h"
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
	if (this->presenter == new_presenter) return;
	if (this->presenter != nullptr && this->pending.has_value()) this->presenter->Hide();
	this->presenter = new_presenter;
	if (this->presenter != nullptr && this->pending.has_value()) this->presenter->Show(*this->pending);
}

bool ConstructionConfirmationController::Submit(ConstructionProposal proposal)
{
	if (!proposal.execute) return false;

	proposal.token = this->next_token++;
	proposal.company = _local_company;
	proposal.map_context = {Map::SizeX(), Map::SizeY(), this->map_epoch};
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
	if (!this->pending.has_value() || this->pending->token != token) return false;
	if (!this->IsValid(*this->pending)) {
		this->Clear();
		return false;
	}

	if (this->pending->validate) {
		const ConstructionValidationResult result = this->pending->validate();
		if (result == ConstructionValidationResult::Invalid) {
			this->Clear();
			return false;
		}
	}

	ConstructionProposal proposal = std::move(*this->pending);
	this->pending.reset();
	this->HidePresenter();
	proposal.execute();
	return true;
}

void ConstructionConfirmationController::Cancel()
{
	if (!this->pending.has_value()) return;
	this->Clear();
}

void ConstructionConfirmationController::Invalidate()
{
	if (!this->pending.has_value()) return;
	this->Clear();
}

void ConstructionConfirmationController::InvalidateForWindow(WindowClass window_class, WindowNumber window_number, uint64_t identity)
{
	if (!this->pending.has_value() || !this->pending->owner.has_value()) return;
	const ConstructionWindowIdentity &owner = *this->pending->owner;
	if (owner.window_class == window_class && owner.window_number == window_number && owner.identity == identity) {
		this->Clear();
	}
}

void ConstructionConfirmationController::NotifyCompanyChanged()
{
	this->Invalidate();
}

void ConstructionConfirmationController::NotifyMapChanged()
{
	this->map_epoch++;
	this->Invalidate();
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

void ConstructionConfirmationController::Clear()
{
	this->pending.reset();
	this->HidePresenter();
}

void ConstructionConfirmationController::HidePresenter()
{
	if (this->presenter != nullptr) this->presenter->Hide();
}

ConstructionConfirmationController &ConstructionConfirmation()
{
	static ConstructionConfirmationController controller;
	return controller;
}

void ShowConstructionConfirmationDemo()
{
	ConstructionProposal proposal;
	proposal.action_type = ConstructionActionType::Apply;
	proposal.label = "Demo construction confirmation (no command)";
	proposal.execute = [] {};
	ConstructionConfirmation().Submit(std::move(proposal));
}
