#include <ch9329.h>

int
ch9329_frame_init(
		struct Ch9329Frame *frame, enum Ch9329Command command, const void *data,
		int len) {
	assert(len <= 256);
	frame->header[0] = 0x57; // STATIC
	frame->header[1] = 0xAB; // STATIC
	frame->header[2] = 0x00; // ADDRESS
	frame->header[3] = command;
	frame->header[4] = len;
	if (len > 0) {
		memcpy(frame->data, data, len);
	}
	return 0;
}

enum Ch9329Command
ch9329_frame_command(const struct Ch9329Frame *frame) {
	return (enum Ch9329Command)frame->header[3];
}

uint8_t
ch9329_frame_len(const struct Ch9329Frame *frame) {
	return frame->header[4];
}

const uint8_t *
ch9329_frame_data(const struct Ch9329Frame *frame) {
	return frame->data;
}

enum Ch9329Error
ch9329_frame_error(const struct Ch9329Frame *frame) {
	if ((ch9329_frame_command(frame) & 0xc0) != 0) {
		return CH9329_SUCCESS;
	} else if (ch9329_frame_len(frame) == 1) {
		return (enum Ch9329Error)frame->data[0];
	} else {
		// TODO
		return CH9329_ERR_OPERATE;
	}
}
