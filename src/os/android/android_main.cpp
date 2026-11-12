/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/gpl-2.0>.
 */

/** @file android_main.cpp Main entry point for the Android build. */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../core/random_func.hpp"
#include "../../string_func.h"

#include <SDL_main.h>
#include <SDL_system.h>

#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>

#include "../../safeguards.h"

/**
 * Entry point called by SDLActivity (through libmain.so -> SDL_main).
 *
 * The working directory and HOME are not provided by Android, so point them at
 * the app-private storage before OpenTTD resolves its personal directories.
 */
int main(int argc, char *argv[])
{
	const char *internal_storage = SDL_AndroidGetInternalStoragePath();
	if (internal_storage != nullptr) {
		std::error_code ec;
		std::filesystem::create_directories(internal_storage, ec);
		setenv("HOME", internal_storage, 1);
		setenv("TMPDIR", internal_storage, 1);
	}

	/* Make sure our arguments contain only valid UTF-8 characters. */
	std::vector<std::string_view> params;
	for (int i = 0; i < argc; ++i) {
		StrMakeValidInPlace(argv[i]);
		params.emplace_back(argv[i]);
	}

	SetRandomSeed(time(nullptr));
	std::signal(SIGPIPE, SIG_IGN);

	return openttd_main(params);
}
