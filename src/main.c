#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <jpeglib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "camera.h"
#include "input.h"

#define MAGIC_KEY SDLK_LALT
#define MAGIC_KEY_TIMEOUT 500
#define COMMAND_MODE_TIMEOUT 3000
#define INDICATOR_TIMEOUT 1000
#define COMMAND_MODE_TIMEOUT_CODE 'D'
#define INDICATOR_TIMEOUT_CODE 'I'

const struct SDL_Color tint_color = {0, 255, 255, SDL_ALPHA_OPAQUE};

const SDL_Color green = {0, 255, 0, SDL_ALPHA_OPAQUE};
const SDL_Color gray = {128, 128, 128, SDL_ALPHA_OPAQUE};
const SDL_Color red = {255, 0, 0, SDL_ALPHA_OPAQUE};

struct Ui {
	bool running;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_TimerID command_mode;
	SDL_TimerID show_indicator;
	struct Camera camera;
	SDL_FRect camera_rect;
	Uint64 last_magic_key_timestamp;
	struct Input input;
};

static bool
draw_indicator(
		struct Ui *ui, const SDL_Point *position,
		bool (*get_status)(struct Input *)) {
	const int width = 8;
	const int height = 32;
	const SDL_Color *color = &red;

	if (input_status_connected(&ui->input)) {
		color = get_status(&ui->input) ? &green : &gray;
	}

	SDL_SetRenderDrawColor(
			ui->renderer, color->r, color->g, color->b, color->a);

	SDL_FRect rect = {
			.x = position->x - width,
			.y = position->y - height,
			.w = width,
			.h = height,
	};

	SDL_RenderFillRect(ui->renderer, &rect);
	return true;
}

static bool
redraw(struct Ui *ui) {
	int window_height = 0;
	int window_width = 0;
	if (!ui->window) {
		return false;
	}

	SDL_LogTrace(SDL_LOG_CATEGORY_RENDER, "Redrawing");

	SDL_Texture *texture = camera_texture(&ui->camera);
	SDL_GetWindowSize(ui->window, &window_width, &window_height);

	SDL_SetRenderDrawColor(
			ui->renderer, tint_color.r / 4, tint_color.g / 4, tint_color.b / 4,
			tint_color.a);
	SDL_RenderClear(ui->renderer);

	if (texture) {
		float aspect_ratio = (float)texture->w / (float)texture->h;
		float win_aspect = (float)window_width / (float)window_height;

		if (win_aspect > aspect_ratio) {
			ui->camera_rect.h = window_height;
			ui->camera_rect.w = (int)(window_height * aspect_ratio);
			ui->camera_rect.x = (window_width - ui->camera_rect.w) / 2;
			ui->camera_rect.y = 0;
		} else {
			ui->camera_rect.w = window_width;
			ui->camera_rect.h = (int)(window_width / aspect_ratio);
			ui->camera_rect.x = 0;
			ui->camera_rect.y = (window_height - ui->camera_rect.h) / 2;
		}
		SDL_RenderTexture(ui->renderer, texture, NULL, &ui->camera_rect);
	}

	if (ui->command_mode) {
		SDL_SetRenderDrawColor(
				ui->renderer, tint_color.r / 2, tint_color.g / 2,
				tint_color.b / 2, tint_color.a);
		SDL_SetRenderScale(ui->renderer, 8.0, 8.0);
		SDL_RenderRect(ui->renderer, NULL);

		SDL_SetRenderDrawColor(
				ui->renderer, tint_color.r / 3, tint_color.g / 3,
				tint_color.b / 3, tint_color.a);
		SDL_SetRenderScale(ui->renderer, 6.0, 6.0);
		SDL_RenderRect(ui->renderer, NULL);

		SDL_SetRenderScale(ui->renderer, 1.0, 1.0);
	}

	if (ui->show_indicator || ui->command_mode) {
		SDL_Point position = {.x = window_width - 32, .y = window_height};
		draw_indicator(ui, &position, input_status_scrolllock);
		position.x -= 64;
		draw_indicator(ui, &position, input_status_capslock);
		position.x -= 64;
		draw_indicator(ui, &position, input_status_numpad);
	}

	SDL_RenderPresent(ui->renderer);

	return true;
}

bool
start_ui(struct Ui *ui) {
	static const int flags = SDL_WINDOW_RESIZABLE;
	int width = 0, height = 0;
	if (ui->window) {
		return true;
	}

	if (!input_start(&ui->input)) {
		return false;
	}

	camera_size(&ui->camera, &width, &height);

	if (!SDL_CreateWindowAndRenderer(
				"kvsm", width, height, flags, &ui->window, &ui->renderer)) {
		SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
		return false;
	}

	float aspect_ratio = (float)width / (float)height;
	SDL_SetWindowAspectRatio(ui->window, aspect_ratio, aspect_ratio);

	return true;
}

static Uint32
user_event_timer(void *userdata, SDL_TimerID timerID, Uint32 interval) {
	(void)timerID;
	(void)interval;
	SDL_Event event = {
			.user = {
					.type = SDL_EVENT_USER,
					.code = (Uint64)userdata,
			}};
	SDL_PushEvent(&event);
	return 0;
}

void
enable_command_mode(struct Ui *ui) {
	if (ui->command_mode) {
		return;
	}

	ui->command_mode = true;
	ui->command_mode = SDL_AddTimer(
			COMMAND_MODE_TIMEOUT, user_event_timer,
			(void *)COMMAND_MODE_TIMEOUT_CODE);
}

bool
handle_command(struct Ui *ui, SDL_Event *event) {
	switch (event->key.key) {
	case SDLK_F: {
		bool fullscreen =
				SDL_GetWindowFlags(ui->window) & SDL_WINDOW_FULLSCREEN;
		SDL_SetWindowFullscreen(ui->window, !fullscreen);
	} break;
	case SDLK_Q:
		ui->running = false;
		break;
	}
	SDL_RemoveTimer(ui->command_mode);
	ui->command_mode = 0;
	redraw(ui);
	return true;
}

int
main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	struct Ui ui = {0};

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA)) {
		SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
		return 1;
	}

	if (!camera_init(&ui.camera, "Openterface: Openterface")) {
		SDL_Log("Couldn't initialize camera: %s", SDL_GetError());
		return 1;
	}

	if (!input_init(&ui.input, "/dev/ttyUSB0")) {
		SDL_Log("Couldn't initialize input: %s", SDL_GetError());
		return 1;
	}

	SDL_Event event;

	ui.running = true;
	while (ui.running && SDL_WaitEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_WINDOW_EXPOSED:
			redraw(&ui);
			break;
		case SDL_EVENT_USER:
			switch (event.user.code) {
			case CAMERA_EVENT_CODE:
				// If we get a camera frame make sure the ui is visible.
				if (!start_ui(&ui)) {
					ui.running = false;
					break;
				}
				camera_update_texture(&ui.camera, ui.renderer);
				break;
			case INPUT_EVENT_CODE:
				SDL_RemoveTimer(ui.show_indicator);
				ui.show_indicator = SDL_AddTimer(
						INDICATOR_TIMEOUT, user_event_timer,
						(void *)INDICATOR_TIMEOUT_CODE);
				redraw(&ui);
				break;
			case COMMAND_MODE_TIMEOUT_CODE:
				SDL_RemoveTimer(ui.command_mode);
				ui.command_mode = 0;
				redraw(&ui);
				break;
			case INDICATOR_TIMEOUT_CODE:
				SDL_RemoveTimer(ui.show_indicator);
				ui.show_indicator = 0;
				redraw(&ui);
				break;
			}
			redraw(&ui);
			break;
		case SDL_EVENT_KEY_DOWN:
			if (ui.command_mode) {
				handle_command(&ui, &event);
			} else {
				input_send_input_event(&ui.input, &event, false);
			}
			if (event.key.key == MAGIC_KEY) {
				Uint64 timestamp = event.key.timestamp / 1000 / 1000;
				if (timestamp - MAGIC_KEY_TIMEOUT <
					ui.last_magic_key_timestamp) {
					enable_command_mode(&ui);
					redraw(&ui);
				}
				ui.last_magic_key_timestamp = timestamp;
			}
			break;
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_WHEEL:
			input_send_input_event(&ui.input, &event, false);
			break;
		case SDL_EVENT_QUIT:
			ui.running = false;
			break;
		case SDL_EVENT_CAMERA_DEVICE_APPROVED:
			SDL_LogTrace(
					SDL_LOG_CATEGORY_APPLICATION,
					"Camera use approved by user!");
			camera_start(&ui.camera);
			break;
		case SDL_EVENT_CAMERA_DEVICE_DENIED:
			SDL_Log("Camera use denied by user!");
			ui.running = false;
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			redraw(&ui);
			break;
		}
	}

	input_cleanup(&ui.input);
	camera_cleanup(&ui.camera);

	SDL_DestroyRenderer(ui.renderer);
	SDL_DestroyWindow(ui.window);
	SDL_Quit();

	return 0;
}
