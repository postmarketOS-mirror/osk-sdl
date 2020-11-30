/*
Copyright (C) 2017 Martijn Braam & Clayton Craft <clayton@craftyguy.net>

This file is part of osk-sdl.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util.h"
#include "draw_helpers.h"
#include <numeric>

int fetchOpts(int argc, char **args, Opts *opts)
{
	int opt;

	while ((opt = getopt(argc, args, "td:n:c:kv")) != -1)
		switch (opt) {
		case 't':
			opts->luksDevPath = DEFAULT_LUKSDEVPATH;
			opts->luksDevName = DEFAULT_LUKSDEVNAME;
			opts->testMode = true;
			break;
		case 'd':
			opts->luksDevPath = optarg;
			break;
		case 'n':
			opts->luksDevName = optarg;
			break;
		case 'c':
			opts->confPath = optarg;
			break;
		case 'k':
			opts->keyscript = true;
			break;
		case 'v':
			opts->verbose = true;
			break;
		default:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Usage: osk-sdl [-t] [-k] [-d /dev/sda] [-n device_name] "
												 "[-c /etc/osk.conf] -v");
			return 1;
		}
	if (opts->luksDevPath.empty()) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No device path specified, use -d [path] or -t");
		return 1;
	}
	if (opts->luksDevName.empty()) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No device name specified, use -n [name] or -t");
		return 1;
	}
	if (opts->confPath.empty()) {
		opts->confPath = DEFAULT_CONFPATH;
	}
	return 0;
}

std::string strVector2str(const std::vector<std::string> &strVector)
{
	const auto strLength = std::accumulate(strVector.begin(), strVector.end(), size_t {},
		[](auto acc, const auto &s) { return acc + s.size(); });
	std::string result;
	result.reserve(strLength);
	for (const auto &str : strVector) {
		result.append(str);
	}
	return result;
}

SDL_Surface *make_wallpaper(Config *config, int width, int height)
{
	SDL_Surface *surface;
	Uint32 rmask, gmask, bmask, amask;

	/* SDL interprets each pixel as a 32-bit number, so our masks must depend
	 on the endianness (byte order) of the machine */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;
#endif

	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, rmask, gmask, bmask, amask);
	SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, config->wallpaper.r, config->wallpaper.g, config->wallpaper.b));
	return surface;
}

void draw_circle(SDL_Renderer *renderer, SDL_Point center, int radius)
{
	int x0 = center.x;
	int y0 = center.y;
	int x = 0;
	int y = radius;
	float d = 5 / 4.0 - radius;
	SDL_SetRenderDrawColor(renderer, 229, 229, 229, 255);

	/*
	 * This is a version of the midpoint circle algorithm that draws lines
	 * between pairs of points for fill, rather than plotting every single
	 * pixel/point within the circle.
	 * https://stackoverflow.com/a/10878576
	 * https://stackoverflow.com/a/1201304
	 * */
	while (x < y) {
		if (d < 2 * (radius - y)) {
			y -= 1;
			d += 2 * y - 1;
		} else if (d >= 2 * x) {
			x += 1;
			d -= 2 * x + 1;
		} else {
			x += 1;
			y -= 1;
			d += 2 * (y - x - 1);
		}
		SDL_RenderDrawLine(renderer, x0 - y, y0 + x, x0 + y, y0 + x);
		SDL_RenderDrawLine(renderer, x0 - x, y0 + y, x0 + x, y0 + y);
		SDL_RenderDrawLine(renderer, x0 - x, y0 - y, x0 + x, y0 - y);
		SDL_RenderDrawLine(renderer, x0 - y, y0 - x, x0 + y, y0 - x);
	}
}

void draw_password_box_dots(SDL_Renderer *renderer, Config *config, int inputHeight, int screenWidth, int numDots, int y, bool busy)
{
	int deflection = inputHeight / 4;
	int ypos = y + inputHeight / 2;
	float tick = static_cast<float>(SDL_GetTicks());
	// Draw password dots
	int dotSize = static_cast<int>(screenWidth * 0.02);
	for (int i = 0; i < numDots; i++) {
		SDL_Point dotPos;
		dotPos.x = (screenWidth / 10) + (i * dotSize * 3);
		if (busy && config->animations) {
			dotPos.y = static_cast<int>(ypos + std::sin(tick / 100.0f + i) * deflection);
		} else {
			dotPos.y = ypos;
		}
		draw_circle(renderer, dotPos, dotSize);
	}
}

bool handleVirtualKeyPress(const std::string &tapped, Keyboard &kbd, LuksDevice &lkd,
	std::vector<std::string> &passphrase, bool keyscript)
{
	// return pressed
	if (tapped == "\n") {
		std::string pass = strVector2str(passphrase);
		lkd.setPassphrase(pass);
		if (keyscript) {
			return true;
		}
		lkd.unlock();
	}
	// Backspace pressed
	else if (tapped == KEYCAP_BACKSPACE) {
		if (!passphrase.empty()) {
			passphrase.pop_back();
		}
	}
	// Shift pressed
	else if (tapped == KEYCAP_SHIFT) {
		if (kbd.getActiveLayer() > 1) {
			kbd.setActiveLayer(0);
		} else {
			kbd.setActiveLayer(!kbd.getActiveLayer());
		}
	}
	// Numbers key pressed:
	else if (tapped == KEYCAP_NUMBERS) {
		kbd.setActiveLayer(2);
	}
	// Symbols key pressed
	else if (tapped == KEYCAP_SYMBOLS) {
		kbd.setActiveLayer(3);
	}
	// ABC key was pressed
	else if (tapped == KEYCAP_ABC) {
		kbd.setActiveLayer(0);
	}
	// handle other key presses
	else if (!tapped.empty()) {
		passphrase.push_back(tapped);
	}
	return false;
}
