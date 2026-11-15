/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file construction_confirmation_diagnostic.h Optional diagnostics for the confirmation bar. */

#ifndef CONSTRUCTION_CONFIRMATION_DIAGNOSTIC_H
#define CONSTRUCTION_CONFIRMATION_DIAGNOSTIC_H

#include "core/format.hpp"
#include "touch_diagnostic.h"

#include <cstdint>
#include <utility>

#ifdef TOUCH_DIAGNOSTIC
inline uint64_t _construction_confirmation_diag_sequence = 0;

/**
 * Write one sequenced construction-confirmation diagnostic line.
 * Only active in TOUCH_DIAGNOSTIC builds.
 */
template <typename... Args>
void ConstructionDiag(fmt::format_string<Args...> format, Args&&... args)
{
	const std::string message = fmt::format(format, std::forward<Args>(args)...);
	TOUCH_DIAGNOSTIC_LOG("confirmation[{}] {}", ++_construction_confirmation_diag_sequence, message);
}
#else
template <typename... Args>
void ConstructionDiag([[maybe_unused]] fmt::format_string<Args...> format, [[maybe_unused]] Args&&... args)
{
}
#endif

#endif /* CONSTRUCTION_CONFIRMATION_DIAGNOSTIC_H */
