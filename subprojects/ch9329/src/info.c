#include "ch9329.h"
#include <assert.h>

int
ch9329_get_info(struct Ch9329 *ch9329, struct Ch9329Frame *frame) {
	ch9329_frame(frame, CH9329_CMD_GET_INFO, NULL, 0);
	return ch9329_request(ch9329, frame);
}

uint8_t
ch9329_info_version(const struct Ch9329Frame *frame) {
	assert(ch9329_frame_command(frame) == CH9329_RES_GET_INFO);
	return ch9329_frame_data(frame)[0];
}

bool
ch9329_info_connected(const struct Ch9329Frame *frame) {
	assert(ch9329_frame_command(frame) == CH9329_RES_GET_INFO);
	return ch9329_frame_data(frame)[1] == 0x01;
}

bool
ch9329_info_num_lock(const struct Ch9329Frame *frame) {
	assert(ch9329_frame_command(frame) == CH9329_RES_GET_INFO);
	return ch9329_frame_data(frame)[1] & CH9329_INDICATOR_NUM_LOCK;
}

bool
ch9329_info_caps_lock(const struct Ch9329Frame *frame) {
	assert(ch9329_frame_command(frame) == CH9329_RES_GET_INFO);
	return ch9329_frame_data(frame)[1] & CH9329_INDICATOR_CAPS_LOCK;
}

bool
ch9329_info_scroll_lock(const struct Ch9329Frame *frame) {
	assert(ch9329_frame_command(frame) == CH9329_RES_GET_INFO);
	return ch9329_frame_data(frame)[1] & CH9329_INDICATOR_SCROLL_LOCK;
}
