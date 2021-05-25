/*
Copyright (C) 2017-2021 Martijn Braam & Clayton Craft <clayton@craftyguy.net>

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

#include "config.h"
#include "util.h"
#include <SDL2/SDL.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

argb parseHexString(const std::string &hex)
{
	argb result = { 255, 0, 0, 0 };
	if (sscanf(hex.c_str(), "#%02hhx%02hhx%02hhx", &result.r, &result.g, &result.b) != 3) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not parse color code %s", hex.c_str());
		exit(EXIT_FAILURE);
	}
	return result;
}

bool Config::Read(const std::string &path)
{
	std::ifstream is(path, std::ifstream::binary);
	if (!is) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not open config file: %s", path.c_str());
		return false;
	}
	if (!Config::Parse(is)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not parse config file: %s", path.c_str());
		return false;
	}

	auto it = Config::options.find("wallpaper");
	if (it != Config::options.end()) {
		std::string hex = Config::options["wallpaper"];
		Config::wallpaper = parseHexString(hex);
	}

	it = Config::options.find("keyboard-background");
	if (it != Config::options.end()) {
		std::string hex = Config::options["keyboard-background"];
		Config::keyboardBackground = parseHexString(hex);
	}

	it = Config::options.find("keyboard-font");
	if (it != Config::options.end()) {
		Config::keyboardFont = Config::options["keyboard-font"];
	}

	it = Config::options.find("keyboard-font-size");
	if (it != Config::options.end()) {
		Config::keyboardFontSize = std::stoi(Config::options["keyboard-font-size"]);
	}

	it = Config::options.find("keyboard-map");
	if (it != Config::options.end()) {
		Config::keyboardMap = Config::options["keyboard-map"];
	}

	it = Config::options.find("key-foreground");
	if (it != Config::options.end()) {
		std::string hex = Config::options["key-foreground"];
		Config::keyForeground = parseHexString(hex);
	}

	it = Config::options.find("key-foreground-highlighted");
	if (it != Config::options.end()) {
		std::string hex = Config::options["key-foreground-highlighted"];
		Config::keyForegroundHighlighted = parseHexString(hex);
	}

	it = Config::options.find("key-background-letter");
	if (it != Config::options.end()) {
		std::string hex = Config::options["key-background-letter"];
		Config::keyBackgroundLetter = parseHexString(hex);
	}

	it = Config::options.find("key-background-return");
	if (it != Config::options.end()) {
		std::string hex = Config::options["key-background-return"];
		Config::keyBackgroundReturn = parseHexString(hex);
	}

	it = Config::options.find("key-background-other");
	if (it != Config::options.end()) {
		std::string hex = Config::options["key-background-other"];
		Config::keyBackgroundOther = parseHexString(hex);
	}

	it = Config::options.find("key-background-highlighted");
	if (it != Config::options.end()) {
		std::string hex = Config::options["key-background-highlighted"];
		Config::keyBackgroundHighlighted = parseHexString(hex);
	}

	it = Config::options.find("key-radius");
	if (it != Config::options.end()) {
		Config::keyRadius = Config::options["key-radius"];
	}

	it = Config::options.find("key-vibrate-duration");
	if (it != Config::options.end()) {
		Config::keyVibrateDuration = std::stoi(Config::options["key-vibrate-duration"]);
	}

	it = Config::options.find("key-preview-popup");
	if (it != Config::options.end()) {
		Config::keyPreview = (Config::options["key-preview-popup"] == "true");
	}

	it = Config::options.find("inputbox-foreground");
	if (it != Config::options.end()) {
		std::string hex = Config::options["inputbox-foreground"];
		Config::inputBoxForeground = parseHexString(hex);
	}

	it = Config::options.find("inputbox-foreground-error");
	if (it != Config::options.end()) {
		std::string hex = Config::options["inputbox-foreground-error"];
		Config::inputBoxForegroundError = parseHexString(hex);
	}

	it = Config::options.find("inputbox-background");
	if (it != Config::options.end()) {
		std::string hex = Config::options["inputbox-background"];
		Config::inputBoxBackground = parseHexString(hex);
	}

	it = Config::options.find("inputbox-background-error");
	if (it != Config::options.end()) {
		std::string hex = Config::options["inputbox-background-error"];
		Config::inputBoxBackgroundError = parseHexString(hex);
	}

	it = Config::options.find("inputbox-radius");
	if (it != Config::options.end()) {
		Config::inputBoxRadius = Config::options["inputbox-radius"];
	}

	it = Config::options.find("animations");
	if (it != Config::options.end()) {
		Config::animations = (Config::options["animations"] == "true");
	}
	return true;
}

bool Config::Parse(std::istream &file)
{
	int lineno = 0;
	for (std::string line; std::getline(file, line);) {
		lineno++;

		std::istringstream iss(line);
		std::string id, eq, val;

		bool error = false;

		if (!(iss >> id)) {
			continue;
		} else if (id[0] == '#') {
			continue;
		} else if (id.empty()) {
			continue;
		} else if (!(iss >> eq >> val >> std::ws) || eq != "=" || iss.get() != EOF) {
			error = true;
		}

		if (error) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Syntax error on line %d", lineno);
			return false;
		} else {
			Config::options[id] = val;
		}
	}
	return true;
}
