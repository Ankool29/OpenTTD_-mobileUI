/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/gpl-2.0>.
 */

/** @file android_main.cpp Main entry point for the Android build. */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../core/format.hpp"
#include "../../core/random_func.hpp"
#include "../../string_func.h"

#include <SDL_main.h>
#include <SDL_system.h>
#include <SDL_version.h>

#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <unwind.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "../../safeguards.h"

/** Log tag used by both the native code and the Java diagnostics. */
static const char *ANDROID_LOG_TAG = "OpenTTD";

/** File descriptor for the current startup log, or -1 when unavailable. */
static int _startup_log_fd = -1;
/** File descriptor for the crash log, or -1 when unavailable. */
static int _crash_log_fd = -1;
/** Serialises writes to the startup log. */
static pthread_mutex_t _log_mutex = PTHREAD_MUTEX_INITIALIZER;
/** Read end of the pipe that receives stdout/stderr. */
static int _stderr_pipe_read = -1;

/**
 * Write one complete line to logcat and to the startup log.
 */
static void AndroidLogLine(int priority, const std::string &message)
{
	__android_log_write(priority, ANDROID_LOG_TAG, message.c_str());

	pthread_mutex_lock(&_log_mutex);
	if (_startup_log_fd >= 0) {
		if (write(_startup_log_fd, message.data(), message.size()) < 0) {
			/* Nothing useful can be done when the log cannot be written. */
		}
		if (write(_startup_log_fd, "\n", 1) < 0) {
			/* Nothing useful can be done when the log cannot be written. */
		}
	}
	pthread_mutex_unlock(&_log_mutex);
}

/**
 * Write a message to logcat and to the startup log.
 */
static void AndroidLog(const std::string &message)
{
	AndroidLogLine(ANDROID_LOG_INFO, message);
}

/**
 * Synchronous error logging for fatal paths that call _Exit() immediately.
 * A pipe reader cannot be relied on to flush before the process disappears.
 */
extern "C" void AndroidLogError(const char *message)
{
	AndroidLogLine(ANDROID_LOG_ERROR, message);
}

/**
 * Forward everything OpenTTD writes to stdout/stderr to logcat and the startup log.
 *
 * OpenTTD reports fatal user errors through stderr, which is otherwise not
 * visible on Android. This makes those messages available for diagnosis.
 */
static void *AndroidStderrThread(void *)
{
	char buffer[1024];
	ssize_t length;
	while ((length = read(_stderr_pipe_read, buffer, sizeof(buffer) - 1)) > 0) {
		buffer[length] = '\0';
		__android_log_write(ANDROID_LOG_INFO, ANDROID_LOG_TAG, buffer);

		pthread_mutex_lock(&_log_mutex);
		if (_startup_log_fd >= 0) {
			if (write(_startup_log_fd, buffer, length) < 0) {
				/* Nothing useful can be done when the log cannot be written. */
			}
		}
		pthread_mutex_unlock(&_log_mutex);
	}
	return nullptr;
}

/**
 * Initialise logging and redirect stdout/stderr into it.
 * @param internal_storage The app-private storage directory.
 */
static void AndroidLogInit(const char *internal_storage)
{
	std::string directory = internal_storage != nullptr ? internal_storage : ".";
	_startup_log_fd = open((directory + "/startup.log").c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
	_crash_log_fd = open((directory + "/crash.log").c_str(), O_CREAT | O_WRONLY | O_APPEND, 0600);

	setvbuf(stdout, nullptr, _IOLBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);

	int pipe_fds[2];
	if (pipe(pipe_fds) != 0) return;

	_stderr_pipe_read = pipe_fds[0];
	dup2(pipe_fds[1], STDOUT_FILENO);
	dup2(pipe_fds[1], STDERR_FILENO);
	close(pipe_fds[1]);

	pthread_t thread;
	if (pthread_create(&thread, nullptr, AndroidStderrThread, nullptr) == 0) {
		pthread_detach(thread);
	}
}

/** State for the unwinder used by the crash handler. */
struct AndroidBacktraceState {
	void **current;
	void **end;
};

static _Unwind_Reason_Code AndroidBacktraceCallback(struct _Unwind_Context *context, void *argument)
{
	AndroidBacktraceState *state = static_cast<AndroidBacktraceState *>(argument);
	uintptr_t pc = _Unwind_GetIP(context);
	if (pc != 0 && state->current < state->end) {
		*state->current++ = reinterpret_cast<void *>(pc);
	}
	return _URC_NO_REASON;
}

/**
 * Log a fatal signal and a raw backtrace, then let Android produce its own tombstone.
 */
static void AndroidCrashHandler(int signal_number, siginfo_t *info, void *)
{
	uintptr_t address = info != nullptr ? reinterpret_cast<uintptr_t>(info->si_addr) : 0;
	std::string text = fmt::format("\nFATAL SIGNAL {}, code={}, address=0x{:x}\n",
			signal_number, info != nullptr ? info->si_code : 0, address);
	if (_crash_log_fd >= 0) {
		if (write(_crash_log_fd, text.data(), text.size()) < 0) {
			/* Nothing useful can be done when the log cannot be written. */
		}
	}
	__android_log_write(ANDROID_LOG_FATAL, ANDROID_LOG_TAG, text.c_str());

	void *frames[64];
	AndroidBacktraceState state{frames, frames + std::size(frames)};
	_Unwind_Backtrace(AndroidBacktraceCallback, &state);

	for (void **frame = frames; frame < state.current; ++frame) {
		Dl_info dl_info;
		if (dladdr(*frame, &dl_info) != 0 && dl_info.dli_sname != nullptr) {
			uintptr_t offset = reinterpret_cast<uintptr_t>(*frame) - reinterpret_cast<uintptr_t>(dl_info.dli_saddr);
			text = fmt::format("  #{} 0x{:x} {}+0x{:x}\n",
					frame - frames, reinterpret_cast<uintptr_t>(*frame), dl_info.dli_sname, offset);
		} else {
			text = fmt::format("  #{} 0x{:x}\n", frame - frames, reinterpret_cast<uintptr_t>(*frame));
		}
		if (_crash_log_fd >= 0) {
			if (write(_crash_log_fd, text.data(), text.size()) < 0) {
				/* Nothing useful can be done when the log cannot be written. */
			}
		}
		__android_log_write(ANDROID_LOG_FATAL, ANDROID_LOG_TAG, text.c_str());
	}

	if (_crash_log_fd >= 0) {
		fsync(_crash_log_fd);
		close(_crash_log_fd);
		_crash_log_fd = -1;
	}

	signal(signal_number, SIG_DFL);
	raise(signal_number);
}

/** Install handlers that make native crashes visible in the logs. */
static void AndroidInstallCrashHandler()
{
	struct sigaction action {};
	action.sa_sigaction = AndroidCrashHandler;
	action.sa_flags = SA_SIGINFO | SA_RESTART;
	sigemptyset(&action.sa_mask);

	for (int signal_number : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) {
		sigaction(signal_number, &action, nullptr);
	}
}

/**
 * Called by the video driver once it enters its main loop.
 *
 * This proves that OpenTTD got past startup and actually reached its user
 * interface; the next launch can then tell a clean exit from an early exit.
 */
extern "C" void AndroidStartupMarkComplete()
{
	static bool marked = false;
	if (marked) return;
	marked = true;
	AndroidLog("STARTUP_COMPLETE");
}

/**
 * Entry point called by SDLActivity (through libmain.so -> SDL_main).
 *
 * The working directory and HOME are not provided by Android, so point them at
 * the app-private storage before OpenTTD resolves its personal directories.
 */
int main(int argc, char *argv[])
{
	const char *internal_storage = SDL_AndroidGetInternalStoragePath();
	AndroidLogInit(internal_storage);

	SDL_version sdl_version;
	SDL_GetVersion(&sdl_version);
	AndroidLog(fmt::format("SDL_main entered: SDL {}.{}.{}, argc={}, internal storage={}",
			sdl_version.major, sdl_version.minor, sdl_version.patch,
			argc, internal_storage != nullptr ? internal_storage : "(null)"));

	AndroidInstallCrashHandler();

	if (internal_storage != nullptr) {
		mkdir(internal_storage, 0700);
		setenv("HOME", internal_storage, 1);
		setenv("TMPDIR", internal_storage, 1);
	}
	AndroidLog(fmt::format("HOME={}", getenv("HOME") != nullptr ? getenv("HOME") : "(unset)"));

	/* Make sure our arguments contain only valid UTF-8 characters. */
	std::vector<std::string_view> params;
	for (int i = 0; i < argc; ++i) {
		StrMakeValidInPlace(argv[i]);
		params.emplace_back(argv[i]);
	}

	SetRandomSeed(time(nullptr));
	signal(SIGPIPE, SIG_IGN);

	AndroidLog("Calling openttd_main");
	int result = openttd_main(params);
	AndroidLog(fmt::format("openttd_main returned {}", result));
	return result;
}
