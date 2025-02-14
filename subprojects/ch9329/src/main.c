#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch9329.h"

static int
get_info(struct Ch9329 *ch9329, int argc, char *argv[]) {
	(void)argv;
	assert(argc == 3);
	struct Ch9329Frame info = {0};
	int rv = ch9329_get_info(ch9329, &info);
	if (rv < 0) {
		goto out;
	}

	printf("Firmware Version: %d\n", ch9329_info_version(&info));
	printf("Connected:        %d\n", ch9329_info_connected(&info));
	printf("Num Lock:         %d\n", ch9329_info_num_lock(&info));
	printf("Caps Lock:        %d\n", ch9329_info_caps_lock(&info));
	printf("Scroll Lock:      %d\n", ch9329_info_scroll_lock(&info));

out:
	return rv;
}

static int
send_key(struct Ch9329 *ch9329, int argc, char *argv[]) {
	int rv = 0;
	assert(argc == 4);
	int scan_code = atoi(argv[3]);
	rv = ch9329_keyboard(ch9329, scan_code, true);
	if (rv < 0) {
		goto out;
	}

	rv = ch9329_keyboard(ch9329, scan_code, false);
	if (rv < 0) {
		goto out;
	}

out:
	return rv;
}

static int
send_string(struct Ch9329 *ch9329, int argc, char *argv[]) {
	assert(argc == 4);
	char *str = argv[3];
	for (int i = 0; str[i] != '\0'; i++) {
		bool upper = false;
		int scan_code = 0;

		if (str[i] >= 'a' && str[i] <= 'z') {
			scan_code = str[i] - 'a' + 4;
		} else if (str[i] >= 'A' && str[i] <= 'Z') {
			scan_code = str[i] - 'A' + 4;
			upper = true;
		} else if (str[i] >= '0' && str[i] <= '9') {
			scan_code = str[i] - '0' + 30;
		} else if (str[i] == ' ') {
			scan_code = 44;
		} else {
			continue;
		}

		if (upper) {
			int rv = ch9329_keyboard(ch9329, 225, true);
			if (rv < 0) {
				return rv;
			}
		}

		int rv = ch9329_keyboard(ch9329, scan_code, true);
		if (rv < 0) {
			return rv;
		}

		rv = ch9329_keyboard(ch9329, scan_code, false);
		if (rv < 0) {
			return rv;
		}

		if (upper) {
			rv = ch9329_keyboard(ch9329, 225, false);
			if (rv < 0) {
				return rv;
			}
		}
	}
	return 0;
}

static int
send_mouse_rel(struct Ch9329 *ch9329, int argc, char *argv[]) {
	assert(argc == 5);
	int x = atoi(argv[3]);
	int y = atoi(argv[4]);
	return ch9329_mouse_rel(ch9329, x, y);
}

static int
send_mouse_abs(struct Ch9329 *ch9329, int argc, char *argv[]) {
	assert(argc == 5);
	int x = atoi(argv[3]);
	int y = atoi(argv[4]);
	return ch9329_mouse_abs(ch9329, x, y);
}

static int
send_wheel(struct Ch9329 *ch9329, int argc, char *argv[]) {
	assert(argc == 4);
	int wheel = atoi(argv[3]);
	return ch9329_mouse_wheel(ch9329, wheel);
}

static int
send_reset(struct Ch9329 *ch9329, int argc, char *argv[]) {
	(void)argv;
	assert(argc == 3);
	return ch9329_reset(ch9329);
}

int
main(int argc, char *argv[]) {
	int rv = 0;
	struct Ch9329 ch9329 = {0};

	assert(argc >= 3);
	rv = ch9329_open(&ch9329, argv[1], 10);
	if (rv < 0) {
		goto out;
	}

	if (strcmp(argv[2], "info") == 0) {
		rv = get_info(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "key") == 0) {
		rv = send_key(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "mouse_rel") == 0) {
		rv = send_mouse_rel(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "mouse_abs") == 0) {
		rv = send_mouse_abs(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "wheel") == 0) {
		rv = send_wheel(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "string") == 0) {
		rv = send_string(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "reset") == 0) {
		rv = send_reset(&ch9329, argc, argv);
	} else {
		rv = -1;
	}

out:
	ch9329_close(&ch9329);
	return rv < 0 ? 1 : 0;
}
