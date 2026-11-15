/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation.h GUI-side confirmation of human construction actions. */

#ifndef CONSTRUCTION_CONFIRMATION_H
#define CONSTRUCTION_CONFIRMATION_H

#include "company_type.h"
#include "economy_type.h"
#include "tilearea_orthogonal.h"
#include "window_type.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

/** The verb shown on the confirmation bar. */
enum class ConstructionActionType : uint8_t {
	Build,   ///< Build something.
	Demolish, ///< Demolish or clear something.
	Apply,   ///< Apply a construction-like operation.
};

/** Result of an optional, non-authoritative proposal validation. */
enum class ConstructionValidationResult : uint8_t {
	Valid,       ///< The proposal passed the optional validation.
	Invalid,     ///< The proposal cannot be confirmed.
	Unavailable, ///< No reliable validation is available; let the command decide.
};

/** Identity of the window that owns a proposal. */
struct ConstructionWindowIdentity {
	WindowClass window_class = WindowClass::Invalid;
	WindowNumber window_number = 0;
	uint64_t identity = 0;
};

/** Map context captured when a proposal is submitted. */
struct ConstructionMapContext {
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t epoch = 0;
};

/** A GUI-level construction proposal. The controller owns it until confirm/cancel/invalidate. */
struct ConstructionProposal {
	ConstructionActionType action_type = ConstructionActionType::Build; ///< Verb for the confirmation button.
	std::string label; ///< Human-readable action description.
	TileArea area{}; ///< Area in tile coordinates. Never store screen coordinates here.
	std::optional<ConstructionWindowIdentity> owner; ///< Optional owner window identity.
	CompanyID company = CompanyID::Invalid(); ///< Company context at submission time.
	ConstructionMapContext map_context{}; ///< Map context at submission time.
	uint32_t tool_id = 0; ///< Opaque tool identifier for future adapters.

	std::function<ConstructionValidationResult()> validate; ///< Optional side-effect-free validation.
	std::function<void()> execute; ///< Deferred action. It must perform the original Post call(s).
	std::optional<Money> estimated_cost; ///< Optional, non-authoritative cost estimate.
	bool keep_tool_on_cancel = true; ///< Policy requested by the adapter; the controller never resets the tool.

	uint64_t token = 0; ///< Assigned by the controller. Adapters must not set this.
};

/** Presentation interface implemented by the Android confirmation bar. */
class ConstructionConfirmationPresenter {
public:
	virtual ~ConstructionConfirmationPresenter() = default;
	virtual void Show(const ConstructionProposal &proposal) = 0;
	virtual void Hide() = 0;
};

/** Controller for pending GUI construction proposals. */
class ConstructionConfirmationController {
public:
	explicit ConstructionConfirmationController(ConstructionConfirmationPresenter *presenter = nullptr);

	void SetPresenter(ConstructionConfirmationPresenter *presenter);

	bool Submit(ConstructionProposal proposal);
	bool Confirm();
	bool Confirm(uint64_t token);
	void Cancel();
	void Invalidate(std::string_view reason = "unspecified");
	void InvalidateForWindow(WindowClass window_class, WindowNumber window_number, uint64_t identity, std::string_view reason = "owner window closed");
	void NotifyCompanyChanged();
	void NotifyMapChanged();

	bool IsPending() const { return this->pending.has_value(); }
	const ConstructionProposal *GetPending() const { return this->pending ? &*this->pending : nullptr; }
	uint64_t GetPendingToken() const { return this->pending ? this->pending->token : 0; }
	uint64_t GetMapEpoch() const { return this->map_epoch; }

private:
	bool IsValid(const ConstructionProposal &proposal) const;
	void Clear(std::string_view reason);
	void HidePresenter();

	std::optional<ConstructionProposal> pending;
	ConstructionConfirmationPresenter *presenter = nullptr;
	uint64_t next_token = 1;
	uint64_t map_epoch = 1;
};

/** Get the process-wide construction confirmation controller. */
ConstructionConfirmationController &ConstructionConfirmation();

/** Submit a no-op demo proposal for manually checking the bar without changing the map. */
void ShowConstructionConfirmationDemo();

/** Attach the confirmation bar presenter to the controller. */
void InitConstructionConfirmationGui();

#endif /* CONSTRUCTION_CONFIRMATION_H */
