/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file touch_diagnostic.h Optional low-frequency touch/placement diagnostics. */

#ifndef TOUCH_DIAGNOSTIC_H
#define TOUCH_DIAGNOSTIC_H

#include "core/format.hpp"

#ifdef TOUCH_DIAGNOSTIC
#	if defined(__ANDROID__)
extern "C" void AndroidTouchDiagnosticLog(const char *message);

#		define TOUCH_DIAGNOSTIC_LOG(...) do { \
			const std::string _touch_diag_message = fmt::format(__VA_ARGS__); \
			AndroidTouchDiagnosticLog(_touch_diag_message.c_str()); \
		} while (false)
#	else
#		include <cstdio>
#		define TOUCH_DIAGNOSTIC_LOG(...) do { \
			fmt::print(stderr, __VA_ARGS__); \
			fmt::print(stderr, "\n"); \
		} while (false)
#	endif
#else
#	define TOUCH_DIAGNOSTIC_LOG(...) do {} while (false)
#endif

#endif /* TOUCH_DIAGNOSTIC_H */
