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

static const SDL_Color color_tint = {0, 255, 255, SDL_ALPHA_OPAQUE};
static const SDL_Color color_green = {0, 255, 0, SDL_ALPHA_OPAQUE};
static const SDL_Color color_gray = {128, 128, 128, SDL_ALPHA_OPAQUE};
static const SDL_Color color_red = {255, 0, 0, SDL_ALPHA_OPAQUE};
static const Uint8 cursor_msb[2][8] = {
		{
				0x00,
				0x60,
				0x60,
				0x00,
				0x00,
				0x00,
				0x00,
				0x00,

		},
		{
				0x60,
				0xF0,
				0xF0,
				0x60,
				0x00,
				0x00,
				0x00,
				0x00,
		},
};

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
	const SDL_Color *color = &color_red;

	if (input_status_connected(&ui->input)) {
		color = get_status(&ui->input) ? &color_green : &color_gray;
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
update_camera_rect(struct Ui *ui) {
	int window_height = 0;
	int window_width = 0;
	SDL_GetWindowSize(ui->window, &window_width, &window_height);
	int camera_width = 0;
	int camera_height = 0;
	if (!camera_size(&ui->camera, &camera_width, &camera_height)) {
		return false;
	}

	float camera_aspect = (float)camera_width / (float)camera_height;
	float win_aspect = (float)window_width / (float)window_height;

	if (win_aspect > camera_aspect) {
		ui->camera_rect.h = window_height;
		ui->camera_rect.w = window_height * camera_aspect;
		ui->camera_rect.x = (window_width - ui->camera_rect.w) / 2;
		ui->camera_rect.y = 0;
	} else {
		ui->camera_rect.w = window_width;
		ui->camera_rect.h = window_width / camera_aspect;
		ui->camera_rect.x = 0;
		ui->camera_rect.y = (window_height - ui->camera_rect.h) / 2;
	}

	input_set_rect(&ui->input, &ui->camera_rect);
	return true;
}

static bool
redraw(struct Ui *ui) {
	if (!ui->window) {
		return false;
	}

	SDL_LogTrace(SDL_LOG_CATEGORY_RENDER, "Redrawing");

	SDL_Texture *texture = camera_texture(&ui->camera);

	SDL_SetRenderDrawColor(
			ui->renderer, color_tint.r / 4, color_tint.g / 4, color_tint.b / 4,
			color_tint.a);
	SDL_RenderClear(ui->renderer);

	if (texture) {
		SDL_RenderTexture(ui->renderer, texture, NULL, &ui->camera_rect);
	}

	if (ui->command_mode) {
		SDL_SetRenderDrawColor(
				ui->renderer, color_tint.r / 2, color_tint.g / 2,
				color_tint.b / 2, color_tint.a);
		SDL_SetRenderScale(ui->renderer, 8.0, 8.0);
		SDL_RenderRect(ui->renderer, NULL);

		SDL_SetRenderDrawColor(
				ui->renderer, color_tint.r / 3, color_tint.g / 3,
				color_tint.b / 3, color_tint.a);
		SDL_SetRenderScale(ui->renderer, 6.0, 6.0);
		SDL_RenderRect(ui->renderer, NULL);

		SDL_SetRenderScale(ui->renderer, 1.0, 1.0);
	}

	if (ui->show_indicator || ui->command_mode ||
		!input_status_connected(&ui->input)) {
		int window_height = 0;
		int window_width = 0;
		SDL_GetWindowSize(ui->window, &window_width, &window_height);

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

	if (!update_camera_rect(ui)) {
		return false;
	}

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
	case SDLK_Q: {
		ui->running = false;
	} break;
	case SDLK_O: {
		SDL_Texture *texture = camera_texture(&ui->camera);
		if (texture) {
			SDL_SetWindowSize(ui->window, texture->w, texture->h);
		}
	} break;
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

	SDL_Cursor *cursor =
			SDL_CreateCursor(cursor_msb[0], cursor_msb[1], 8, 8, 1, 1);
	SDL_SetCursor(cursor);

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
				break;
			case COMMAND_MODE_TIMEOUT_CODE:
				SDL_RemoveTimer(ui.command_mode);
				ui.command_mode = 0;
				break;
			case INDICATOR_TIMEOUT_CODE:
				SDL_RemoveTimer(ui.show_indicator);
				ui.show_indicator = 0;
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
			SDL_LogWarn(
					SDL_LOG_CATEGORY_APPLICATION, "Camera use denied by user!");
			ui.running = false;
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			update_camera_rect(&ui);
			redraw(&ui);
			break;
		}
	}

	input_cleanup(&ui.input);
	camera_cleanup(&ui.camera);

	SDL_DestroyRenderer(ui.renderer);
	SDL_DestroyWindow(ui.window);
	SDL_DestroyCursor(cursor);
	SDL_Quit();

	return 0;
}
