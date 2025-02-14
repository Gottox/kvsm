#include "input.h"
#include "SDL3/SDL_scancode.h"
#include <ch9329.h>
#include <stdbool.h>
#include <sys/select.h>

#define MOD_LCTRL (1 << 0)
#define MOD_LSHIFT (1 << 1)
#define MOD_LALT (1 << 2)
#define MOD_LGUI (1 << 3)
#define MOD_RCTRL (1 << 4)
#define MOD_RSHIFT (1 << 5)
#define MOD_RALT (1 << 6)
#define MOD_RGUI (1 << 7)

#define DEBOUNCE_THRESHOULD 5

bool
input_init(struct Input *input, const char *input_name) {
	bool rv = false;
	input->mutex = SDL_CreateMutex();
	if (input->mutex == NULL) {
		goto out;
	}
	input->condition_mutex = SDL_CreateMutex();
	if (input->condition_mutex == NULL) {
		goto out;
	}
	input->condition = SDL_CreateCondition();
	if (input->condition == NULL) {
		goto out;
	}

	input->status_interval = 100;

	if (ch9329_open(&input->hid, input_name, 100) < 0) {
		goto out;
	}

	rv = true;
out:
	if (!rv) {
		input_cleanup(input);
	}
	return rv;
}

static bool
button(struct Input *input, Uint8 button, bool pressed) {
	Uint8 button_mask = 0;
	switch (button) {
	case SDL_BUTTON_LEFT:
		button_mask = CH9329_MOUSE_LEFT;
		break;
	case SDL_BUTTON_MIDDLE:
		button_mask = CH9329_MOUSE_MIDDLE;
		break;
	case SDL_BUTTON_RIGHT:
		button_mask = CH9329_MOUSE_RIGHT;
		break;
	default:
		return false;
	}

	return ch9329_mouse_button(&input->hid, button_mask, pressed) >= 0;
}

static bool
mouse_abs(struct Input *input, Uint16 x, Uint16 y) {
	x = (x - input->rect.x) * 4096 / input->rect.w;
	y = (y - input->rect.y) * 4096 / input->rect.h;
	if (x > 4095) {
		x = 4095;
	}
	if (y > 4095) {
		y = 4095;
	}
	return ch9329_mouse_abs(&input->hid, x, y) >= 0;
}

static bool
handle_input_event(struct Input *input, struct InputEvent *event) {
	switch (event->event.type) {
	case SDL_EVENT_KEY_DOWN:
		ch9329_keyboard(&input->hid, event->event.key.scancode, true);
		break;
	case SDL_EVENT_KEY_UP:
		ch9329_keyboard(&input->hid, event->event.key.scancode, false);
		break;
	case SDL_EVENT_MOUSE_MOTION:
		mouse_abs(input, event->event.motion.x, event->event.motion.y);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		button(input, event->event.button.button, true);
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		button(input, event->event.button.button, false);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		ch9329_mouse_wheel(&input->hid, event->event.wheel.y);
		break;
	}
	return true;
}

static bool
status_update(struct Input *input) {
	bool rv = false;
	bool changed;
	struct Ch9329Frame frame = {0};
	rv = ch9329_get_info(&input->hid, &frame) >= 0;
	if (!rv) {
		goto out;
	}

	SDL_LockMutex(input->mutex);
	changed = 0 !=
			SDL_memcmp(&input->hid_status, &frame, sizeof(struct Ch9329Frame));
	if (changed) {
		SDL_memcpy(&input->hid_status, &frame, sizeof(struct Ch9329Frame));
	}
	SDL_UnlockMutex(input->mutex);

	if (changed) {
		SDL_Event event = {
				.user = {
						.type = SDL_EVENT_USER,
						.code = INPUT_EVENT_CODE,
				}};
		SDL_PushEvent(&event);
	}

	rv = true;
out:
	return rv;
}

static int
debounce_events(struct Input *input) {
	bool debounce = true;
	while (debounce) {
		SDL_LockMutex(input->mutex);
		struct InputEvent *event_item =
				&input->event_ring[input->event_ring_last_head];
		if (event_item->event.type != SDL_EVENT_MOUSE_MOTION) {
			debounce = false;
		} else if (!SDL_WaitConditionTimeout(
						   input->condition, input->condition_mutex,
						   DEBOUNCE_THRESHOULD)) {
			debounce = false;
		}
		SDL_UnlockMutex(input->mutex);
	}
	return 0;
}

static int
input_thread(void *data) {
	struct Input *input = data;
	SDL_LockMutex(input->condition_mutex);

	while (true) {
		struct InputEvent *event_item = NULL;
		if (SDL_WaitConditionTimeout(
					input->condition, input->condition_mutex,
					input->status_interval)) {
			debounce_events(input);

			SDL_LockMutex(input->mutex);
			while (input->event_ring_tail != input->event_ring_head) {
				event_item = &input->event_ring[input->event_ring_tail];
				input->event_ring_tail =
						(input->event_ring_tail + 1) % INPUT_EVENT_RING_SIZE;
				SDL_UnlockMutex(input->mutex);
				handle_input_event(input, event_item);
				memset(event_item, 0, sizeof(struct InputEvent));
				SDL_LockMutex(input->mutex);
			}
			SDL_UnlockMutex(input->mutex);
		} else if (input->status_interval == 0) {
			break;
		} else {
			if (event_item) {
				handle_input_event(input, event_item);
				memset(event_item, 0, sizeof(struct InputEvent));
			} else {
				status_update(input);
			}
		}
	}

	SDL_UnlockMutex(input->condition_mutex);
	return 0;
}

bool
input_start(struct Input *input) {
	bool rv = false;

	rv = status_update(input);
	if (!rv) {
		goto out;
	}

	input->thread = SDL_CreateThread(input_thread, "input_thread", input);
	if (input->thread == NULL) {
		goto out;
	}
out:
	return rv;
}

bool
input_send_input_event(struct Input *input, SDL_Event *event, bool rel_mouse) {
	SDL_LockMutex(input->condition_mutex);

	struct InputEvent *last_event_item =
			&input->event_ring[input->event_ring_last_head];
	if (event->type == SDL_EVENT_MOUSE_MOTION &&
		last_event_item->event.type == SDL_EVENT_MOUSE_MOTION) {
		SDL_memcpy(&last_event_item->event, event, sizeof(SDL_Event));
	} else {
		struct InputEvent *event_item =
				&input->event_ring[input->event_ring_head];
		int next_head = (input->event_ring_head + 1) % INPUT_EVENT_RING_SIZE;
		SDL_memcpy(&event_item->event, event, sizeof(SDL_Event));
		event_item->rel_mouse = rel_mouse;
		input->event_ring_last_head = input->event_ring_head;
		input->event_ring_head = next_head;
	}

	SDL_SignalCondition(input->condition);

	SDL_UnlockMutex(input->condition_mutex);
	return true;
}

bool
input_status_numpad(struct Input *input) {
	SDL_LockMutex(input->mutex);
	bool rv = ch9329_frame_data(&input->hid_status)[2] & 0x01;
	SDL_UnlockMutex(input->mutex);
	return rv;
}

bool
input_status_capslock(struct Input *input) {
	SDL_LockMutex(input->mutex);
	bool rv = ch9329_frame_data(&input->hid_status)[2] & 0x02;
	SDL_UnlockMutex(input->mutex);
	return rv;
}

bool
input_status_scrolllock(struct Input *input) {
	SDL_LockMutex(input->mutex);
	bool rv = ch9329_frame_data(&input->hid_status)[2] & 0x04;
	SDL_UnlockMutex(input->mutex);
	return rv;
}

bool
input_status_connected(struct Input *input) {
	SDL_LockMutex(input->mutex);
	bool rv = ch9329_frame_data(&input->hid_status)[1] == 0x01;
	SDL_UnlockMutex(input->mutex);
	return rv;
}

bool
input_set_rect(struct Input *input, SDL_FRect *rect) {
	SDL_LockMutex(input->mutex);
	input->rect = *rect;
	SDL_UnlockMutex(input->mutex);
	return true;
}

bool
input_cleanup(struct Input *input) {
	input->status_interval = 0;
	SDL_SignalCondition(input->condition);
	SDL_WaitThread(input->thread, NULL);

	SDL_DestroyMutex(input->mutex);
	SDL_DestroyMutex(input->condition_mutex);
	SDL_DestroyCondition(input->condition);
	ch9329_close(&input->hid);
	return true;
}
