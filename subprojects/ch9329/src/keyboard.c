#include "ch9329.h"
#include <string.h>

int
ch9329_keyboard(struct Ch9329 *ch9329, uint8_t scan_code, bool pressed) {
	struct Ch9329Frame frame = {0};

	// Is a modifier
	if (scan_code >= 224 && scan_code <= 231) {
		int bit = scan_code - 224;
		if (pressed) {
			ch9329->keyboard_state[0] |= 1 << bit;
		} else {
			ch9329->keyboard_state[0] &= ~(1 << bit);
		}
	} else if (pressed) {
		for (int i = 2; i < 8; i++) {
			if (ch9329->keyboard_state[i] == 0 ||
				ch9329->keyboard_state[i] == scan_code) {
				ch9329->keyboard_state[i] = scan_code;
			}
		}
	} else {
		for (int i = 2; i < 8; i++) {
			if (ch9329->keyboard_state[i] == scan_code) {
				ch9329->keyboard_state[i] = 0;
			}
		}
	}

	ch9329_frame(
			&frame, CH9329_CMD_SEND_KB_GENERAL_DATA, ch9329->keyboard_state, 8);
	return ch9329_request(ch9329, &frame);
}
