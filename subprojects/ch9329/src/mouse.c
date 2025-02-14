#include "ch9329.h"

int
ch9329_mouse_abs(struct Ch9329 *ch9329, uint16_t x, uint16_t y) {
	const uint8_t data[7] = {
			[0] = 0x02,     [1] = ch9329->mouse_button_state,
			[2] = x & 0xff, [3] = (x >> 8) & 0xff,
			[4] = y & 0xff, [5] = (y >> 8) & 0xff,
			[6] = 0x00,
	};
	struct Ch9329Frame frame = {0};
	ch9329_frame(&frame, CH9329_CMD_SEND_MS_ABS_DATA, &data, 7);
	return ch9329_request(ch9329, &frame);
}

int
ch9329_mouse_rel(struct Ch9329 *ch9329, int8_t x, int8_t y) {
	const uint8_t data[5] = {
			[0] = 0x01, [1] = ch9329->mouse_button_state, [2] = x, [3] = y,
			[4] = 0x00,
	};
	struct Ch9329Frame frame = {0};
	ch9329_frame(&frame, CH9329_CMD_SEND_MS_REL_DATA, &data, 5);
	return ch9329_request(ch9329, &frame);
}

int
ch9329_mouse_wheel(struct Ch9329 *ch9329, int8_t wheel) {
	const uint8_t data[5] = {
			[0] = 0x01,  [1] = ch9329->mouse_button_state, [2] = 0, [3] = 0,
			[4] = wheel,
	};
	struct Ch9329Frame frame = {0};
	ch9329_frame(&frame, CH9329_CMD_SEND_MS_REL_DATA, &data, 5);
	return ch9329_request(ch9329, &frame);
}

int
ch9329_mouse_button(struct Ch9329 *ch9329, uint8_t button, bool pressed) {
	if (pressed) {
		ch9329->mouse_button_state |= button & CH9329_MOUSE_ALL;
	} else {
		ch9329->mouse_button_state &= ~button;
	}
	return ch9329_mouse_rel(ch9329, 0, 0);
}
