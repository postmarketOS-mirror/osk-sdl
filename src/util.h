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

#ifndef UTIL_H
#define UTIL_H
#include "config.h"
#include "keyboard.h"
#include "luksdevice.h"
#include <SDL2/SDL.h>
#include <cmath>
#include <iostream>
#include <string>
#include <unistd.h>

constexpr char DEFAULT_LUKSDEVPATH[] = "/home/user/disk";
constexpr char DEFAULT_LUKSDEVNAME[] = "root";
constexpr char DEFAULT_CONFPATH[] = "/etc/osk.conf";

struct Opts {
	std::string luksDevPath;
	std::string luksDevName;
	std::string confPath;
	std::string confOverridePath;
	bool testMode;
	bool softwareRendering;
	bool verbose;
	bool keyscript;
	bool noGLES;
};

/**
  Fetch command line options
  @param argc Number of arguments passed on cmdline
  @param args Arguments from cmdline
  @param opts Structure for storing options from cmdline
  @return 0 on success, non-zero on failure
 */
int fetchOpts(int argc, char **args, Opts *opts);

/**
  Convert vector of strings into a single string
  @param strVector Vector of strings
  @return String with all elements of strVector concatenated together
 */
std::string strVector2str(const std::vector<std::string> &strVector);

/**
  Return the index of the OpenGL ES driver
  @return The driver's index or -1 (the default driver) when no OpenGL ES driver can be found
 */
int find_gles_driver_index();

/**
  Create wallpaper
  @param config Config paramters
  @param width Width of wallpaper to generate
  @param height Height of wallpaper to generate
  @return Initialized SDL_Surface, else nullptr on failure
 */
SDL_Surface *make_wallpaper(Config *config, int width, int height);

/**
  Draw a circle
  @param renderer Initialized SDL_Renderer object
  @param center Center position of circle
  @param radius Radius of circle
  @param fillColor Fill color of circle
 */
void draw_circle(SDL_Renderer *renderer, SDL_Point center, int radius, const argb &fillColor);

/**
  Handle keypresses for virtual keyboard
  @param tapped Character tapped on keyboard
  @param kbd Initialized Keyboard obj
  @param lkd Initialized LuksDevice obj
  @param lkd passphrase Passphrase to modify
  @param keyscript Whether we're in keyscript mode
  @return Whether we're done with the main loop
 */
bool handleVirtualKeyPress(const std::string &tapped, Keyboard &kbd, LuksDevice &lkd,
	std::vector<std::string> &passphrase, bool keyscript);

/**
  Draw the dots to represent hidden characters
  @param renderer Initialized SDL_Renderer object
  @param inputRect Bounding box of the password input
  @param numDots Number of password 'dots' to draw
  @param busy if true the dots will play a loading animation
 */
void draw_password_box_dots(SDL_Renderer *renderer, Config *config, const SDL_Rect &inputRect, int numDots, bool busy);

/**
  Handle a finger or mouse down event
  @param xTapped X coordinate of the tap
  @param yTapped Y coordinate of the tap
  @param screenHeight Height of overall screen
  @param kbd Initialized Keyboard obj
 */
void handleTapBegin(unsigned xTapped, unsigned yTapped, int screenHeight, Keyboard &kbd);

/**
  Handle a finger or mouse up event
  @param xTapped X coordinate of the tap
  @param yTapped Y coordinate of the tap
  @param screenHeight Height of overall screen
  @param kbd Initialized Keyboard obj
  @param lkd Initialized LuksDevice obj
  @param passphrase The current passphrase
  @param keyscript Whether we're in keyscript mode
  @param showPasswordError Will be set to true if a password error should be shown, false otherwise
  @param done Will be set to true if the device was unlocked, false otherwise
 */
void handleTapEnd(unsigned xTapped, unsigned yTapped, int screenHeight, Keyboard &kbd, LuksDevice &lkd, std::vector<std::string> &passphrase, bool keyscript, bool &showPasswordError, bool &done);

/**
  Rumble a haptic device for the given duration
  @param haptic Initialized haptic device
  @param config Config paramters
 */
void hapticRumble(SDL_Haptic *haptic, Config *config);

/**
  Determine if the app is using software rendering
  @return true if using SW rendering, else false
 */
bool isSwRenderer(SDL_Renderer *renderer);
#endif
