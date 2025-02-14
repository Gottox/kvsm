#include "ch9329.h"

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

int
ch9329_keyboard_extra(
		struct Ch9329 *ch9329, enum Ch9329KeyboardExtra key, bool pressed) {
	uint8_t *data;
	uint32_t mask = (uint32_t)1
			<< (key & ~(CH9329_ACPI_KEY_MASK | CH9329_MEDIA_KEY_MASK));
	struct Ch9329Frame frame = {0};

	if (key & CH9329_ACPI_KEY_MASK) {
		data = ch9329->acpi_key_state;
		data[0] = 0x01;
	} else if (key & CH9329_MEDIA_KEY_MASK) {
		data = ch9329->media_key_state;
		data[0] = 0x02;
	} else {
		return -1;
	}

	if (pressed) {
		data[1] |= (mask >> 0) & 0xff;
		data[2] |= (mask >> 8) & 0xff;
		data[3] |= (mask >> 16) & 0xff;
	} else {
		data[1] &= ~((mask >> 0) & 0xff);
		data[2] &= ~((mask >> 8) & 0xff);
		data[3] &= ~((mask >> 16) & 0xff);
	}

	ch9329_frame(&frame, CH9329_CMD_SEND_KB_MEDIA_DATA, data, 4);
	return ch9329_request(ch9329, &frame);
}
