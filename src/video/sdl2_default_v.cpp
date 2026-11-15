/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sdl2_default_v.cpp Implementation of the default backend for SDL2 video driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../error_func.h"
#include "../gfx_func.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../thread.h"
#include "../progress.h"
#include "../core/geometry_func.hpp"
#include "../fileio_func.h"
#include "../framerate_type.h"
#include "../window_func.h"
#include "../construction_confirmation.h"
#include "../construction_confirmation_diagnostic.h"
#include "sdl2_default_v.h"
#include <SDL.h>
#include <mutex>
#include <condition_variable>
#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <emscripten/html5.h>
#endif

#include "../safeguards.h"

#ifdef TOUCH_DIAGNOSTIC
/** Read one SDL surface pixel and return packed RGBA. */
static uint32_t SampleSDLSurfacePixel(SDL_Surface *surface, int x, int y, bool &valid)
{
	valid = false;
	if (surface == nullptr || x < 0 || y < 0 || x >= surface->w || y >= surface->h) return 0;
	if (SDL_LockSurface(surface) != 0) return 0;

	uint32_t raw = 0;
	switch (surface->format->BytesPerPixel) {
		case 1: raw = *reinterpret_cast<uint8_t *>(static_cast<uint8_t *>(surface->pixels) + y * surface->pitch + x); break;
		case 2: raw = *reinterpret_cast<uint16_t *>(static_cast<uint8_t *>(surface->pixels) + y * surface->pitch + x * 2); break;
		case 3: {
			const uint8_t *p = static_cast<uint8_t *>(surface->pixels) + y * surface->pitch + x * 3;
			raw = p[0] | (p[1] << 8) | (p[2] << 16);
			break;
		}
		case 4: raw = *reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(surface->pixels) + y * surface->pitch + x * 4); break;
		default: SDL_UnlockSurface(surface); return 0;
	}
	SDL_UnlockSurface(surface);

	uint8_t r, g, b, a;
	SDL_GetRGBA(raw, surface->format, &r, &g, &b, &a);
	valid = true;
	return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(b) << 8) | a;
}
#endif

#ifdef TOUCH_DIAGNOSTIC
static uint32_t _sdl_confirm_paint_count = 0;
#endif

static FVideoDriver_SDL_Default iFVideoDriver_SDL_Default; ///< The default, non-GL, SDL video driver.

static SDL_Surface *_sdl_surface; ///< The surface to draw on/to.
static SDL_Surface *_sdl_rgb_surface; ///< Optional surface to use for blitting all changes at once.
static SDL_Surface *_sdl_real_surface; ///< The actual surface of the screen.
static SDL_Palette *_sdl_palette; ///< The palette mapping.

/**
 * Update the dirty palette colours.
 */
void VideoDriver_SDL_Default::UpdatePalette()
{
	SDL_Color pal[256];

	for (int i = 0; i != this->local_palette.count_dirty; i++) {
		pal[i].r = this->local_palette.palette[this->local_palette.first_dirty + i].r;
		pal[i].g = this->local_palette.palette[this->local_palette.first_dirty + i].g;
		pal[i].b = this->local_palette.palette[this->local_palette.first_dirty + i].b;
		pal[i].a = 0;
	}

	SDL_SetPaletteColors(_sdl_palette, pal, this->local_palette.first_dirty, this->local_palette.count_dirty);
	SDL_SetSurfacePalette(_sdl_surface, _sdl_palette);
}

/**
 * Allocate and configure the palette.
 */
void VideoDriver_SDL_Default::MakePalette()
{
	if (_sdl_palette == nullptr) {
		_sdl_palette = SDL_AllocPalette(256);
		if (_sdl_palette == nullptr) UserError("SDL2: Couldn't allocate palette: {}", SDL_GetError());
	}

	CopyPalette(this->local_palette, true);
	this->UpdatePalette();

	if (_sdl_surface != _sdl_real_surface) {
		/* When using a shadow surface, also set our palette on the real screen. This lets SDL
		 * allocate as many colours (or approximations) as
		 * possible, instead of using only the default SDL
		 * palette. This allows us to get more colours exactly
		 * right and might allow using better approximations for
		 * other colours.
		 *
		 * Note that colours allocations are tried in-order, so
		 * this favors colours further up into the palette. Also
		 * note that if two colours from the same animation
		 * sequence are approximated using the same colour, that
		 * animation will stop working.
		 *
		 * Since changing the system palette causes the colours
		 * to change right away, and allocations might
		 * drastically change, we can't use this for animation,
		 * since that could cause weird colouring between the
		 * palette change and the blitting below, so we only set
		 * the real palette during initialisation.
		 */
		SDL_SetSurfacePalette(_sdl_real_surface, _sdl_palette);
	}
}

void VideoDriver_SDL_Default::Paint()
{
	PerformanceMeasurer framerate(PerformanceElement::Video);

	const bool confirm_diag = FindWindowById(WindowClass::ConstructionConfirmation, 0) != nullptr;
#ifdef TOUCH_DIAGNOSTIC
	const bool log_confirm = confirm_diag && (_sdl_confirm_paint_count < 12 || _sdl_confirm_paint_count % 60 == 0);
	if (confirm_diag) ++_sdl_confirm_paint_count;
#else
	const bool log_confirm = false;
#endif
	if (log_confirm) {
		int window_w = 0, window_h = 0;
		SDL_GetWindowSize(this->sdl_window, &window_w, &window_h);
		ConstructionDiag("SDL Paint: dirty=({},{}) {}x{} screen={}x{} window={}x{} surface={}x{} real={}x{} different_surface={} empty={}",
				this->dirty_rect.left, this->dirty_rect.top, this->dirty_rect.right - this->dirty_rect.left, this->dirty_rect.bottom - this->dirty_rect.top,
				_screen.width, _screen.height, window_w, window_h,
				_sdl_surface != nullptr ? _sdl_surface->w : -1, _sdl_surface != nullptr ? _sdl_surface->h : -1,
				_sdl_real_surface != nullptr ? _sdl_real_surface->w : -1, _sdl_real_surface != nullptr ? _sdl_real_surface->h : -1,
				_sdl_surface != _sdl_real_surface, IsEmptyRect(this->dirty_rect));

		Window *bar = FindWindowById(WindowClass::ConstructionConfirmation, 0);
		Window *statusbar = FindWindowById(WindowClass::Statusbar, 0);
		if (bar != nullptr) {
			bool s1, s2, s3;
			const int bx = bar->left + 10;
			const int by = bar->top + 10;
			const int cx = bar->left + bar->width / 2;
			const int cy = bar->top + bar->height / 2;
			const uint32_t surface1 = SampleSDLSurfacePixel(_sdl_surface, bx, by, s1);
			const uint32_t surface2 = SampleSDLSurfacePixel(_sdl_surface, cx, cy, s2);
			const uint32_t real1 = SampleSDLSurfacePixel(_sdl_real_surface, bx, by, s3);
			ConstructionDiag("SDL surface before update: bar=({},{}) {}x{} surface={:08X}/{}/{:08X}/{}/real={:08X}/{} statusbar_top={}",
					bar->left, bar->top, bar->width, bar->height, surface1, s1, surface2, s2, real1, s3,
					statusbar != nullptr ? statusbar->top : -1);
		}
	}

	if (IsEmptyRect(this->dirty_rect) && this->local_palette.count_dirty == 0) return;

	if (this->local_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PaletteAnimation::VideoBackend:
				this->UpdatePalette();
				break;

			case Blitter::PaletteAnimation::Blitter: {
				blitter->PaletteAnimate(this->local_palette);
				break;
			}

			case Blitter::PaletteAnimation::None:
				break;

			default:
				NOT_REACHED();
		}
		this->local_palette.count_dirty = 0;
	}

	SDL_Rect r = { this->dirty_rect.left, this->dirty_rect.top, this->dirty_rect.right - this->dirty_rect.left, this->dirty_rect.bottom - this->dirty_rect.top };

	if (_sdl_surface != _sdl_real_surface) {
		SDL_BlitSurface(_sdl_surface, &r, _sdl_real_surface, &r);
	}
	const int update_result = SDL_UpdateWindowSurfaceRects(this->sdl_window, &r, 1);
	if (log_confirm) {
		ConstructionDiag("SDL update rect=({},{}) {}x{} result={} error='{}'", r.x, r.y, r.w, r.h, update_result, SDL_GetError());
		Window *bar = FindWindowById(WindowClass::ConstructionConfirmation, 0);
		if (bar != nullptr) {
			bool s1, s2, s3;
			const int bx = bar->left + 10;
			const int by = bar->top + 10;
			const int cx = bar->left + bar->width / 2;
			const int cy = bar->top + bar->height / 2;
			const uint32_t source1 = SampleSDLSurfacePixel(_sdl_surface, bx, by, s1);
			const uint32_t source2 = SampleSDLSurfacePixel(_sdl_surface, cx, cy, s2);
			const uint32_t real1 = SampleSDLSurfacePixel(_sdl_real_surface, bx, by, s3);
			ConstructionDiag("SDL surface after update: source={:08X}/{}/{:08X}/{}/real={:08X}/{}",
					source1, s1, source2, s2, real1, s3);
		}
	}

	this->dirty_rect = {};
}

bool VideoDriver_SDL_Default::AllocateBackingStore(int w, int h, bool force)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	_sdl_real_surface = SDL_GetWindowSurface(this->sdl_window);
	if (_sdl_real_surface == nullptr) UserError("SDL2: Couldn't get window surface: {}", SDL_GetError());

	if (!force && w == _sdl_real_surface->w && h == _sdl_real_surface->h) return false;

	/* Free any previously allocated rgb surface. */
	if (_sdl_rgb_surface != nullptr) {
		SDL_FreeSurface(_sdl_rgb_surface);
		_sdl_rgb_surface = nullptr;
	}

	if (bpp == 8) {
		_sdl_rgb_surface = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
		if (_sdl_rgb_surface == nullptr) UserError("SDL2: Couldn't allocate shadow surface: {}", SDL_GetError());

		_sdl_surface = _sdl_rgb_surface;
	} else {
		_sdl_surface = _sdl_real_surface;
	}

	/* X11 doesn't appreciate it if we invalidate areas outside the window
	 * if shared memory is enabled (read: it crashes). So, as we might have
	 * gotten smaller, reset our dirty rects. GameSizeChanged() a bit lower
	 * will mark the whole screen dirty again anyway, but this time with the
	 * new dimensions. */
	this->dirty_rect = {};

	_screen.width = _sdl_surface->w;
	_screen.height = _sdl_surface->h;
	_screen.pitch = _sdl_surface->pitch / (bpp / 8);
	_screen.dst_ptr = this->GetVideoPointer();

	this->MakePalette();

	return true;
}

void *VideoDriver_SDL_Default::GetVideoPointer()
{
	return _sdl_surface->pixels;
}
