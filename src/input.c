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

static bool
select_hid(struct Input *input) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(input->hid.fd, &fds);
	struct timeval tv = {0};
	tv.tv_sec = 0;
	tv.tv_usec = input->receive_timeout * 1000;
	return select(input->hid.fd + 1, &fds, NULL, NULL, &tv) > 0;
}

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
	input->receive_timeout = 100;

	if (ch9329_open(&input->hid, input_name) < 0) {
		goto out;
	}

	rv = true;
out:
	if (!rv) {
		input_cleanup(input);
	}
	return rv;
}

static void
add_key_state(struct Input *input, SDL_Scancode scancode) {
	switch (scancode) {
	case SDL_SCANCODE_LCTRL:
		input->key_data[0] |= MOD_LCTRL;
		break;
	case SDL_SCANCODE_LSHIFT:
		input->key_data[0] |= MOD_LSHIFT;
		break;
	case SDL_SCANCODE_LALT:
		input->key_data[0] |= MOD_LALT;
		break;
	case SDL_SCANCODE_LGUI:
		input->key_data[0] |= MOD_LGUI;
		break;
	case SDL_SCANCODE_RCTRL:
		input->key_data[0] |= MOD_RCTRL;
		break;
	case SDL_SCANCODE_RSHIFT:
		input->key_data[0] |= MOD_RSHIFT;
		break;
	case SDL_SCANCODE_RALT:
		input->key_data[0] |= MOD_RALT;
		break;
	case SDL_SCANCODE_RGUI:
		input->key_data[0] |= MOD_RGUI;
		break;
	default:
		for (int i = 2; i < INPUT_KEY_DATA_SIZE; i++) {
			if (input->key_data[i] == 0) {
				input->key_data[i] = scancode;
				break;
			}
		}
		break;
	}
}

static void
remove_key_state(struct Input *input, SDL_Scancode scancode) {
	switch (scancode) {
	case SDL_SCANCODE_LCTRL:
		input->key_data[0] &= ~MOD_LCTRL;
		break;
	case SDL_SCANCODE_LSHIFT:
		input->key_data[0] &= ~MOD_LSHIFT;
		break;
	case SDL_SCANCODE_LALT:
		input->key_data[0] &= ~MOD_LALT;
		break;
	case SDL_SCANCODE_LGUI:
		input->key_data[0] &= ~MOD_LGUI;
		break;
	case SDL_SCANCODE_RCTRL:
		input->key_data[0] &= ~MOD_RCTRL;
		break;
	case SDL_SCANCODE_RSHIFT:
		input->key_data[0] &= ~MOD_RSHIFT;
		break;
	case SDL_SCANCODE_RALT:
		input->key_data[0] &= ~MOD_RALT;
		break;
	case SDL_SCANCODE_RGUI:
		input->key_data[0] &= ~MOD_RGUI;
		break;
	default:
		for (int i = 2; i < INPUT_KEY_DATA_SIZE; i++) {
			if (input->key_data[i] == scancode) {
				input->key_data[i] = 0;
				break;
			}
		}
		break;
	}
}

static bool
update_keyboard(struct Input *input) {
	bool rv = false;
	struct Ch9329Frame frame = {0};
	ch9329_frame_init(
			&frame, CH9329_CMD_SEND_KB_GENERAL_DATA, input->key_data,
			INPUT_KEY_DATA_SIZE);

	if (!ch9329_send(&input->hid, &frame)) {
		goto out;
	}

	if (!select_hid(input)) {
		SDL_Log("Timeout waiting for keyboard update");
		goto out;
	}

	if (ch9329_receive(&input->hid, &frame) < 0) {
		goto out;
	}

	rv = true;
out:
	return rv;
}

static bool
update_mouse(struct Input *input, Sint16 xrel, Sint16 yrel) {
	bool rv = false;
	uint8_t data[4] = {0};
	data[0] = xrel & 0xff;
	data[1] = (xrel >> 8) & 0xff;
	data[2] = yrel & 0xff;
	data[3] = (yrel >> 8) & 0xff;

	struct Ch9329Frame frame = {0};
	ch9329_frame_init(&frame, CH9329_CMD_SEND_MS_REL_DATA, data, 4);

	if (!ch9329_send(&input->hid, &frame)) {
		goto out;
	}

	if (!select_hid(input)) {
		SDL_Log("Timeout waiting for mouse update");
		goto out;
	}

	if (ch9329_receive(&input->hid, &frame) < 0) {
		goto out;
	}

	rv = true;
out:
	return rv;
}

static bool
handle_input_event(struct Input *input, struct InputEvent *event) {
	switch (event->event.type) {
	case SDL_EVENT_KEY_DOWN:
		add_key_state(input, event->event.key.scancode);
		update_keyboard(input);
		break;
	case SDL_EVENT_KEY_UP:
		remove_key_state(input, event->event.key.scancode);
		update_keyboard(input);
		break;
	case SDL_EVENT_MOUSE_MOTION:
		// update_mouse(input, event->rel_mouse, event->event.motion.xrel,
		// event->event.motion.yrel);
		break;
	}
	return true;
}

static bool
status_update(struct Input *input) {
	bool rv = false;
	bool changed;
	struct Ch9329Frame frame = {0};

	ch9329_frame_init(&frame, CH9329_CMD_GET_INFO, NULL, 0);

	if (ch9329_send(&input->hid, &frame) < 0) {
		goto out;
	}
	if (select_hid(input) == false) {
		SDL_Log("Timeout waiting for input status");
		goto out;
	}
	if (ch9329_receive(&input->hid, &frame) < 0) {
		goto out;
	}

	if (ch9329_frame_error(&frame) != CH9329_SUCCESS) {
		goto out;
	}
	if (ch9329_frame_len(&frame) != 8) {
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
input_thread(void *data) {
	struct Input *input = data;
	SDL_LockMutex(input->condition_mutex);

	while (true) {
		struct InputEvent *event_item = NULL;
		if (SDL_WaitConditionTimeout(
					input->condition, input->condition_mutex,
					input->status_interval)) {
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
	struct Ch9329Frame frame = {0};
	input->thread = SDL_CreateThread(input_thread, "input_thread", input);
	if (input->thread == NULL) {
		goto out;
	}

	if (ch9329_send(&input->hid, &frame) < 0) {
		goto out;
	}

	rv = status_update(input);
out:
	return rv;
}

bool
input_send_input_event(struct Input *input, SDL_Event *event, bool rel_mouse) {
	bool rv = false;
	SDL_LockMutex(input->condition_mutex);

	int next_head = (input->event_ring_head + 1) % INPUT_EVENT_RING_SIZE;
	if (next_head == input->event_ring_tail) {
		SDL_LogWarn(
				SDL_LOG_CATEGORY_APPLICATION,
				"Input event ring full, dropping event");
		goto out;
	}
	struct InputEvent *event_item = &input->event_ring[input->event_ring_head];
	input->event_ring_head = next_head;

	SDL_memcpy(&event_item->event, event, sizeof(SDL_Event));
	event_item->rel_mouse = rel_mouse;

	SDL_SignalCondition(input->condition);

	rv = true;
out:
	SDL_UnlockMutex(input->condition_mutex);
	return rv;
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
