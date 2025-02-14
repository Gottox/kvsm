#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch9329.h"

static int
get_info(struct Ch9329 *ch9329, int argc, char *argv[]) {
	if (argc != 3) {
		puts("Usage: ch9329 <DEVICE> info");
		return -1;
	}
	(void)argv;
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
send_media(struct Ch9329 *ch9329, int argc, char *argv[]) {
	static const struct {
		char *name;
		uint32_t code;
	} codes[] = {
#define X(code) {#code, code}
			X(CH9329_MEDIA_KEY_VOLUME_UP),     X(CH9329_MEDIA_KEY_VOLUME_DOWN),
			X(CH9329_MEDIA_KEY_MUTE),          X(CH9329_MEDIA_KEY_PLAY_PAUSE),
			X(CH9329_MEDIA_KEY_NEXT_TRACK),    X(CH9329_MEDIA_KEY_PREV_TRACK),
			X(CH9329_MEDIA_KEY_CD_STOP),       X(CH9329_MEDIA_KEY_EJECT),
			X(CH9329_MEDIA_KEY_EMAIL),         X(CH9329_MEDIA_KEY_WWW_SEARCH),
			X(CH9329_MEDIA_KEY_WWW_FAVORITES), X(CH9329_MEDIA_KEY_WWW_HOME),
			X(CH9329_MEDIA_KEY_WWW_BACK),      X(CH9329_MEDIA_KEY_WWW_FORWARD),
			X(CH9329_MEDIA_KEY_WWW_STOP),      X(CH9329_MEDIA_KEY_REFRESH),
			X(CH9329_MEDIA_KEY_MEDIA),         X(CH9329_MEDIA_KEY_EXPLORER),
			X(CH9329_MEDIA_KEY_CALCULATOR),    X(CH9329_MEDIA_KEY_SCREEN_SAVE),
			X(CH9329_MEDIA_KEY_MY_COMPUTER),   X(CH9329_MEDIA_KEY_MINIMIZE),
			X(CH9329_MEDIA_KEY_RECORD),        X(CH9329_MEDIA_KEY_REWIND),

			X(CH9329_ACPI_KEY_POWER),          X(CH9329_ACPI_KEY_SLEEP),
			X(CH9329_ACPI_KEY_WAKEUP),         {NULL, 0},
#undef X
	};

	if (argc != 4) {
		puts("Usage: ch9329 <DEVICE> key CODE");
		printf("Codes:\n");
		for (size_t i = 0; codes[i].name; i++) {
			printf("  %s\n", codes[i].name);
		}

		return -1;
	}

	int rv = 0;
	int code = 0;

	for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
		if (strcmp(argv[3], codes[i].name) == 0) {
			code = codes[i].code;
			break;
		}
	}
	if (code == 0) {
		puts("Unknown code");
		goto out;
	}

	rv = ch9329_keyboard_extra(ch9329, code, true);
	if (rv < 0) {
		goto out;
	}

	rv = ch9329_keyboard_extra(ch9329, code, false);
	if (rv < 0) {
		goto out;
	}

out:
	return rv;
}

static int
send_key(struct Ch9329 *ch9329, int argc, char *argv[]) {
	if (argc != 4) {
		puts("Usage: ch9329 <DEVICE> key SCAN_CODE");
		return -1;
	}
	int rv = 0;
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
	if (argc != 4) {
		puts("Usage: ch9329 <DEVICE> string STRING");
		return -1;
	}
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
	if (argc != 5) {
		puts("Usage: ch9329 <DEVICE> mouse_rel X Y");
		return -1;
	}
	int x = atoi(argv[3]);
	int y = atoi(argv[4]);
	return ch9329_mouse_rel(ch9329, x, y);
}

static int
send_mouse_abs(struct Ch9329 *ch9329, int argc, char *argv[]) {
	if (argc != 5) {
		puts("Usage: ch9329 <DEVICE> mouse_abs X Y");
		return -1;
	}
	int x = atoi(argv[3]);
	int y = atoi(argv[4]);
	return ch9329_mouse_abs(ch9329, x, y);
}

static int
send_click(struct Ch9329 *ch9329, int argc, char *argv[]) {
	if (argc != 4) {
		puts("Usage: ch9329 <DEVICE> click BUTTON");
		return -1;
	}
	int rv = 0;
	int button = atoi(argv[3]);
	rv = ch9329_mouse_button(ch9329, button, true);
	if (rv < 0) {
		goto out;
	}

	rv = ch9329_mouse_button(ch9329, button, false);
	if (rv < 0) {
		goto out;
	}

out:
	return rv;
}

static int
send_wheel(struct Ch9329 *ch9329, int argc, char *argv[]) {
	if (argc != 4) {
		puts("Usage: ch9329 <DEVICE> wheel WHEEL");
		return -1;
	}
	int wheel = atoi(argv[3]);
	return ch9329_mouse_wheel(ch9329, wheel);
}

static int
send_reset(struct Ch9329 *ch9329, int argc, char *argv[]) {
	(void)argv;
	if (argc != 3) {
		puts("Usage: ch9329 <DEVICE> reset");
		return -1;
	}
	assert(argc == 3);
	return ch9329_reset(ch9329);
}

int
main(int argc, char *argv[]) {
	int rv = 0;
	struct Ch9329 ch9329 = {0};

	if (argc < 3) {
		puts("Usage: ch9329 <DEVICE> COMMAND [ARGS...]");
		puts("Commands:");
		puts("info");
		puts("mouse_rel X Y");
		puts("mouse_abs X Y");
		puts("wheel WHEEL");
		puts("key SCAN_CODE");
		puts("string STRING");
		puts("reset");
		return 1;
	}

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
	} else if (strcmp(argv[2], "click") == 0) {
		rv = send_click(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "wheel") == 0) {
		rv = send_wheel(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "string") == 0) {
		rv = send_string(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "media") == 0) {
		rv = send_media(&ch9329, argc, argv);
	} else if (strcmp(argv[2], "reset") == 0) {
		rv = send_reset(&ch9329, argc, argv);
	} else {
		puts("Unknown command");
		rv = -1;
	}

out:
	ch9329_close(&ch9329);
	return rv < 0 ? 1 : 0;
}
