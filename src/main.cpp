/*
Copyright (C) 2017-2021
Martijn Braam, Clayton Craft <clayton@craftyguy.net>, et al.

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
#include "draw_helpers.h"
#include "keyboard.h"
#include "luksdevice.h"
#include "tooltip.h"
#include "util.h"
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <list>
#include <string>
#include <unistd.h>

bool lastUnlockingState = false;
bool showPasswordError = false;
constexpr char ErrorText[] = "Incorrect passphrase";
constexpr char EnterPassText[] = "Enter disk decryption passphrase";
constexpr char UnlockingDiskText[] = "Trying to unlock disk...";

int main(int argc, char **args)
{
	std::vector<std::string> passphrase;
	Opts opts {};
	Config config;
	SDL_Event event;
	SDL_Window *display = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Haptic *haptic = nullptr;
	int WIDTH = 480;
	int HEIGHT = 800;
	std::chrono::milliseconds repeat_delay { 25 }; // Keyboard key repeat rate in ms
	unsigned prev_keydown_ticks = 0; // Two sep. prev_ticks required for handling
	unsigned prev_text_ticks = 0; // textinput & keydown event types

	static Uint32 renderEventType = SDL_RegisterEvents(1);
	static SDL_Event renderEvent {
		.type = renderEventType
	};

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_ERROR);
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

	if (fetchOpts(argc, args, &opts)) {
		exit(EXIT_FAILURE);
	}

	if (opts.verbose) {
		SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
	}

	SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "osk-sdl v%s", VERSION);

	if (!config.Read(opts.confPath)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No valid config file specified, use -c [path]");
		exit(EXIT_FAILURE);
	}

	if (!opts.confOverridePath.empty() && !config.Read(opts.confOverridePath)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Config override file could not be loaded, continuing");
	}

	/* Disable animations when using software rendering, overriding what's in the config */
	config.animations = config.animations && !opts.softwareRendering;
	config.keyPreview = config.keyPreview && !opts.softwareRendering;
	SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Enabling animations: %s",
		    config.animations ? "true" : "false");

	LuksDevice luksDev(opts.luksDevName, opts.luksDevPath, renderEventType);

	atexit(SDL_Quit);
	Uint32 sdlFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_HAPTIC;

	if (SDL_Init(sdlFlags) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Init failed: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (!opts.testMode) {
		// Switch to the resolution of the framebuffer if not running
		// in test mode.
		SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, nullptr };
		if (SDL_GetDisplayMode(0, 0, &mode) != 0) {
			SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "SDL_GetDisplayMode failed: %s", SDL_GetError());
			exit(EXIT_FAILURE);
		}
		WIDTH = mode.w;
		HEIGHT = mode.h;
	}

	/*
	 * Set up display and renderer
	 * Use windowed mode in test mode and device resolution otherwise
	 */
	Uint32 windowFlags = 0;
	if (opts.testMode) {
		windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
	} else {
		windowFlags = SDL_WINDOW_FULLSCREEN;
	}

	display = SDL_CreateWindow("OSK SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
		windowFlags);
	if (display == nullptr) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create window/display: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/*
	  * Prefer using GLES, since it's better supported on mobile devices
	  * than full GL.
	  * NOTE: sw rendering using GL, not GLES
	  */
	int rendererIndex = -1;
	if (!opts.noGLES && !opts.softwareRendering)
		rendererIndex = find_gles_driver_index();
	int rendererFlags = 0;
	if (opts.softwareRendering) {
		rendererFlags |= SDL_RENDERER_SOFTWARE;
	}
	renderer = SDL_CreateRenderer(display, rendererIndex, rendererFlags);

	if (renderer == nullptr) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create renderer: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	int keyboardHeight = HEIGHT / 3 * 2;
	if (HEIGHT > WIDTH) {
		// Keyboard height is screen width / max number of keys per row * rows
		// Denominator below chosen to provide enough room for a 5 row layout without causing key height to
		// shrink too much
		keyboardHeight = WIDTH / 1.6;
	}

	int inputWidth = static_cast<int>(WIDTH * 0.9);
	int inputHeight = static_cast<int>(WIDTH * 0.1);

	if (SDL_SetRenderDrawColor(renderer, 255, 128, 0, SDL_ALPHA_OPAQUE) != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not set background color: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (SDL_RenderFillRect(renderer, nullptr) != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not fill background color: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// Disable mouse cursor if not in testmode
	if (SDL_ShowCursor(opts.testMode) < 0) {
		SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Setting cursor visibility failed: %s", SDL_GetError());
		// Not stopping here, this is a pretty recoverable error.
	}

	// Initialize haptic device
	if (SDL_WasInit(SDL_INIT_HAPTIC) != 0) {
		haptic = SDL_HapticOpen(0);
		if (haptic == nullptr) {
			SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Unable to open haptic device");
		} else if (SDL_HapticRumbleInit(haptic) != 0) {
			SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Unable to initialize haptic device");
			SDL_HapticClose(haptic);
			haptic = nullptr;
		} else {
			SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Initialized haptic device");
		}
	}

	// Initialize virtual keyboard
	Keyboard keyboard(0, 1, WIDTH, keyboardHeight, &config, haptic);
	if (keyboard.init(renderer)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to initialize keyboard!");
		exit(EXIT_FAILURE);
	}

	// Make SDL send text editing events for textboxes
	SDL_StartTextInput();

	SDL_Surface *wallpaper = make_wallpaper(&config, WIDTH, HEIGHT);
	SDL_Texture *wallpaperTexture = SDL_CreateTextureFromSurface(renderer, wallpaper);

	int inputBoxRadius = std::strtol(config.inputBoxRadius.c_str(), nullptr, 10);
	if (inputBoxRadius >= BEZIER_RESOLUTION || inputBoxRadius > inputHeight / 1.5) {
		SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "inputbox-radius must be below %d and %f, it is %d", BEZIER_RESOLUTION,
			inputHeight / 1.5, inputBoxRadius);
		inputBoxRadius = 0;
	}

	// Initialize tooltip for password error
	Tooltip passErrorTooltip(TooltipType::error, inputWidth, inputHeight, inputBoxRadius, &config);
	if (passErrorTooltip.init(renderer, ErrorText)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to initialize passErrorTooltip!");
		exit(EXIT_FAILURE);
	}

	Tooltip enterPassTooltip(TooltipType::info, inputWidth, inputHeight, inputBoxRadius, &config);
	if (enterPassTooltip.init(renderer, EnterPassText)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to initialize enterPassTooltip!");
		exit(EXIT_FAILURE);
	}

	// Tooltip for unlocking (when animations are disabled)
	Tooltip unlockingTooltip(TooltipType::info, inputWidth, inputHeight, inputBoxRadius, &config);
	if (unlockingTooltip.init(renderer, UnlockingDiskText)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to initialize unlockingTooltip!");
		exit(EXIT_FAILURE);
	}

	argb inputBoxColor = config.inputBoxBackground;

	SDL_Surface *inputBox = make_input_box(inputWidth, inputHeight, &inputBoxColor, inputBoxRadius);
	SDL_Texture *inputBoxTexture = SDL_CreateTextureFromSurface(renderer, inputBox);

	int topHalf = static_cast<int>(HEIGHT - (keyboard.getHeight() * keyboard.getPosition()));
	SDL_Rect inputBoxRect = SDL_Rect {
		.x = WIDTH / 20,
		.y = static_cast<int>(topHalf / 3.5),
		.w = inputWidth,
		.h = inputHeight
	};

	if (inputBoxTexture == nullptr) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create input box texture: %s",
			SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_RendererInfo rendererInfo;
	SDL_GetRendererInfo(renderer, &rendererInfo);

	SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Using software rendering: %s",
		    (rendererInfo.flags & SDL_RENDERER_SOFTWARE) ? "true" : "false");

	// Start drawing keyboard when main loop starts
	SDL_PushEvent(&renderEvent);

	// The Main Loop.
	bool done = false;
	int cur_ticks = 0;
	while (luksDev.isLocked() && !done) {
		if (SDL_WaitEvent(&event)) {
			// an event was found
			switch (event.type) {
			// handle the keyboard
			case SDL_KEYDOWN:
				// handle repeat key events
				cur_ticks = SDL_GetTicks();
				if ((cur_ticks - repeat_delay.count()) < prev_keydown_ticks) {
					continue;
				}
				showPasswordError = false;
				prev_keydown_ticks = cur_ticks;
				switch (event.key.keysym.sym) {
				case SDLK_RETURN:
					if (!passphrase.empty() && !luksDev.unlockRunning()) {
						std::string pass = strVector2str(passphrase);
						luksDev.setPassphrase(pass);
						if (opts.keyscript) {
							done = true;
						} else {
							luksDev.unlock();
						}
					}
					break; // SDLK_RETURN
				case SDLK_BACKSPACE:
					if (!passphrase.empty() && !luksDev.unlockRunning()) {
						passphrase.pop_back();
						SDL_PushEvent(&renderEvent);
						continue;
					}
					break; // SDLK_BACKSPACE
				case SDLK_ESCAPE:
					goto QUIT;
					break; // SDLK_ESCAPE
				}
				SDL_PushEvent(&renderEvent);
				break; // SDL_KEYDOWN
				// handle touchscreen
			case SDL_FINGERDOWN: {
				// x and y values are normalized!
				auto xTouch = static_cast<unsigned>(event.tfinger.x * WIDTH);
				auto yTouch = static_cast<unsigned>(event.tfinger.y * HEIGHT);
				handleTapBegin(xTouch, yTouch, HEIGHT, keyboard);
				SDL_PushEvent(&renderEvent);
				break; // SDL_FINGERDOWN
			}
			case SDL_FINGERUP: {
				auto xTouch = static_cast<unsigned>(event.tfinger.x * WIDTH);
				auto yTouch = static_cast<unsigned>(event.tfinger.y * HEIGHT);
				handleTapEnd(xTouch, yTouch, HEIGHT, keyboard, luksDev, passphrase, opts.keyscript, showPasswordError, done);
				SDL_PushEvent(&renderEvent);
				break; // SDL_FINGERUP
			}
				// handle the mouse
			case SDL_MOUSEBUTTONDOWN: {
				handleTapBegin(event.button.x, event.button.y, HEIGHT, keyboard);
				SDL_PushEvent(&renderEvent);
				break; // SDL_MOUSEBUTTONDOWN
			}
			case SDL_MOUSEBUTTONUP: {
				handleTapEnd(event.button.x, event.button.y, HEIGHT, keyboard, luksDev, passphrase, opts.keyscript, showPasswordError, done);
				SDL_PushEvent(&renderEvent);
				break; // SDL_MOUSEBUTTONUP
			}
				// handle physical keyboard
			case SDL_TEXTINPUT: {
				/*
				 * Only register text input if time since last text input has exceeded
				 * the keyboard repeat delay rate
				 */
				showPasswordError = false;
				cur_ticks = SDL_GetTicks();
				// Enable key repeat delay
				if ((cur_ticks - repeat_delay.count()) > prev_text_ticks) {
					prev_text_ticks = cur_ticks;
					if (!luksDev.unlockRunning()) {
						passphrase.emplace_back(event.text.text);
						SDL_LogInfo(SDL_LOG_CATEGORY_INPUT, "Phys Keyboard Key Entered %s", event.text.text);
					}
				}
				SDL_PushEvent(&renderEvent);
				break; // SDL_TEXTINPUT
			}
			case SDL_QUIT:
				SDL_Log("Quit requested, quitting.");
				exit(0);
				break; // SDL_QUIT
			} // switch event.type
			// Render event handler
			if (event.type == renderEventType) {
				/* NOTE ON MULTI BUFFERING / RENDERING MULTIPLE TIMES:
				   We only re-schedule render events during animation, otherwise
				   we render once and then do nothing for a long while.

				   A single render may however never reach the screen, since
				   SDL_RenderCopy() page flips and with multi buffering that
				   may just fill the hidden backbuffer(s).

				   Therefore, we need to render multiple times if not during
				   animation to make sure it actually shows on screen during
				   lengthy pauses.

				   For software rendering (directfb backend), rendering twice
				   seems to be the sweet spot.

				   For accelerated rendering, we render 3 times to make sure
				   updates show on screen for drivers that use
				   triple buffering
				 */
				int render_times = 0;
				int max_render_times = (rendererInfo.flags & SDL_RENDERER_ACCELERATED) ? 3 : 2;
				while (render_times < max_render_times) {
					render_times++;
					SDL_RenderCopy(renderer, wallpaperTexture, nullptr, nullptr);

					topHalf = static_cast<int>(HEIGHT - (keyboard.getHeight() * keyboard.getPosition()));
					inputBoxRect.y = static_cast<int>(topHalf / 3.5);
					// Only show either error tooltip, enter password tooltip, or password input box
					if (showPasswordError) {
						passErrorTooltip.draw(renderer, inputBoxRect.x, inputBoxRect.y);
					} else if (passphrase.size() == 0) {
						enterPassTooltip.draw(renderer, inputBoxRect.x, inputBoxRect.y);
					} else if (luksDev.unlockRunning() && !config.animations) {
						unlockingTooltip.draw(renderer, inputBoxRect.x, inputBoxRect.y);
					} else {
						SDL_RenderCopy(renderer, inputBoxTexture, nullptr, &inputBoxRect);
						draw_password_box_dots(renderer, &config, inputBoxRect, passphrase.size(), luksDev.unlockRunning());
					}

					// Hide keyboard if unlock luks thread is running
					keyboard.setTargetPosition(!luksDev.unlockRunning());
					// Draw keyboard last so that key previews don't get drawn over by e.g. the input box
					keyboard.draw(renderer, HEIGHT);
					SDL_RenderPresent(renderer);
					if (keyboard.isInSlideAnimation()) {
						// No need to double-flip if we'll redraw more for animation
						// in a tiny moment anyway.
						break;
					}
				}

				if (lastUnlockingState != luksDev.unlockRunning()) {
					if (!luksDev.unlockRunning() && luksDev.isLocked()) {
						// Luks is finished and the password was wrong
						showPasswordError = true;
						passphrase.clear();
						// Show default keyboard layer again on wrong passphrase
						keyboard.setActiveLayer(0);
						SDL_PushEvent(&renderEvent);
					}
					lastUnlockingState = luksDev.unlockRunning();
				}
				// If any animations are enabled and running, continue to push render events to the
				// event queue
				if (config.animations && (luksDev.unlockRunning() || keyboard.isInSlideAnimation())) {
					SDL_PushEvent(&renderEvent);
				}
			}
		} // event handle loop
	} // main loop

QUIT:
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_HAPTIC);

	if (opts.keyscript) {
		std::string pass = strVector2str(passphrase);
		printf("%s", pass.c_str());
		fflush(stdout);
	}
	return 0;
}
