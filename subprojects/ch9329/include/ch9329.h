#ifndef CH9329_H
#define CH9329_H
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

typedef int SerialPort;

struct Ch9329 {
	SerialPort fd;
	struct termios oldtio;
	int timeout;
	uint8_t keyboard_state[8];
	uint8_t mouse_button_state;
};

struct Ch9329Frame {
	uint8_t header[5];
	uint8_t data[256];
};

enum Ch9329Indicator {
	CH9329_INDICATOR_NUM_LOCK = 1,
	CH9329_INDICATOR_CAPS_LOCK = 1 << 1,
	CH9329_INDICATOR_SCROLL_LOCK = 1 << 2,
};

enum Ch9329Command {
	CH9329_CMD_GET_INFO = 0x01,
	CH9329_CMD_SEND_KB_GENERAL_DATA = 0x02,
	CH9329_CMD_SEND_KB_MEDIA_DATA = 0x03,
	CH9329_CMD_SEND_MS_ABS_DATA = 0x04,
	CH9329_CMD_SEND_MS_REL_DATA = 0x05,
	CH9329_CMD_SEND_MY_HID_DATA = 0x06,
	CH9329_CMD_READ_MY_HID_DATA = 0x87,
	CH9329_CMD_GET_PARA_CFG = 0x08,
	CH9329_CMD_SET_PARA_CFG = 0x09,
	CH9329_CMD_GET_USB_STRING = 0x0A,
	CH9329_CMD_SET_USB_STRING = 0x0B,
	CH9329_CMD_SET_DEFAULT_CFG = 0x0C,
	CH9329_CMD_RESET = 0x0F,

	CH9329_RES_GET_INFO = CH9329_CMD_GET_INFO | 0x80,
	CH9329_RES_SEND_KB_GENERAL_DATA = CH9329_CMD_SEND_KB_GENERAL_DATA | 0x80,
	CH9329_RES_SEND_KB_MEDIA_DATA = CH9329_CMD_SEND_KB_MEDIA_DATA | 0x80,
	CH9329_RES_SEND_MS_ABS_DATA = CH9329_CMD_SEND_MS_ABS_DATA | 0x80,
	CH9329_RES_SEND_MS_REL_DATA = CH9329_CMD_SEND_MS_REL_DATA | 0x80,
	CH9329_RES_SEND_MY_HID_DATA = CH9329_CMD_SEND_MY_HID_DATA | 0x80,
	CH9329_RES_READ_MY_HID_DATA = CH9329_CMD_READ_MY_HID_DATA | 0x80,
	CH9329_RES_GET_PARA_CFG = CH9329_CMD_GET_PARA_CFG | 0x80,
	CH9329_RES_SET_PARA_CFG = CH9329_CMD_SET_PARA_CFG | 0x80,
	CH9329_RES_GET_USB_STRING = CH9329_CMD_GET_USB_STRING | 0x80,
	CH9329_RES_SET_USB_STRING = CH9329_CMD_SET_USB_STRING | 0x80,
	CH9329_RES_SET_DEFAULT_CFG = CH9329_CMD_SET_DEFAULT_CFG | 0x80,
	CH9329_RES_RESET = CH9329_CMD_RESET | 0x80,
};

enum Ch9329Error {
	CH9329_SUCCESS = 0x00, // Command executed successfully
	CH9329_ERR_TIMEOUT = 0xE1, // Serial port reception timeout for a byte
	CH9329_ERR_HEAD = 0xE2, // Serial port received an incorrect header byte
	CH9329_ERR_CMD = 0xE3, // Serial port received an incorrect command code
	CH9329_ERR_SUM = 0xE4, // Checksum mismatch
	CH9329_ERR_PARA = 0xE5, // Parameter error
	CH9329_ERR_OPERATE = 0xE6 // Normal operation, but execution failed
};

////////////////////////////////////////
// frame.c
int ch9329_frame(
		struct Ch9329Frame *frame, enum Ch9329Command command, const void *data,
		int len);

enum Ch9329Command ch9329_frame_command(const struct Ch9329Frame *frame);

uint8_t ch9329_frame_len(const struct Ch9329Frame *frame);

const uint8_t *ch9329_frame_data(const struct Ch9329Frame *frame);

enum Ch9329Error ch9329_frame_error(const struct Ch9329Frame *frame);

////////////////////////////////////////
// ch9329.c
int ch9329_init(struct Ch9329 *ch9329, int fd, int timeout);

int ch9329_open(struct Ch9329 *ch9329, const char *path, int timeout);

int ch9329_receive(struct Ch9329 *ch9329, struct Ch9329Frame *frame);

int ch9329_send(struct Ch9329 *ch9329, const struct Ch9329Frame *frame);

int ch9329_request(struct Ch9329 *ch9329, struct Ch9329Frame *frame);

int ch9329_reset(struct Ch9329 *ch9329);

int ch9329_close(struct Ch9329 *ch9329);

////////////////////////////////////////
// info.c
int ch9329_get_info(struct Ch9329 *ch9329, struct Ch9329Frame *frame);

uint8_t ch9329_info_version(const struct Ch9329Frame *frame);

bool ch9329_info_connected(const struct Ch9329Frame *frame);

bool ch9329_info_num_lock(const struct Ch9329Frame *frame);

bool ch9329_info_caps_lock(const struct Ch9329Frame *frame);

bool ch9329_info_scroll_lock(const struct Ch9329Frame *frame);

////////////////////////////////////////
// keyboard.c
int ch9329_keyboard(struct Ch9329 *ch9329, uint8_t scan_code, bool pressed);

////////////////////////////////////////
// mouse.c
enum Ch9329MouseButton {
	CH9329_MOUSE_LEFT = 1,
	CH9329_MOUSE_RIGHT = 1 << 1,
	CH9329_MOUSE_MIDDLE = 1 << 2,
	CH9329_MOUSE_ALL =
			CH9329_MOUSE_LEFT | CH9329_MOUSE_RIGHT | CH9329_MOUSE_MIDDLE,
};

int ch9329_mouse_abs(struct Ch9329 *ch9329, uint16_t x, uint16_t y);

int ch9329_mouse_rel(struct Ch9329 *ch9329, int8_t x, int8_t y);

int ch9329_mouse_wheel(struct Ch9329 *ch9329, int8_t wheel);

int ch9329_mouse_button(struct Ch9329 *ch9329, uint8_t button, bool pressed);

#endif
