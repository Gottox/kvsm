#include <stdio.h>

#include "ch9329.h"

int
main(int argc, char *argv[]) {
	int rv = 0;
	struct Ch9329 ch9329;

	assert(argc == 2);
	rv = ch9329_open(&ch9329, argv[1]);
	if (rv < 0) {
		goto out;
	}

	struct Ch9329Frame frame = {0};
	ch9329_frame_init(&frame, CH9329_CMD_GET_INFO, "", 0);

	rv = ch9329_send(&ch9329, &frame);
	if (rv < 0) {
		printf("Failed to send frame\n");
		perror("send");
		goto out;
	}

	rv = ch9329_receive(&ch9329, &frame);
	if (rv < 0) {
		printf("Failed to receive frame\n");
		goto out;
	}

	for (int i = 0; i < 5; i++) {
		printf("%02x ", frame.header[i]);
	}
	for (int i = 0; i < ch9329_frame_len(&frame); i++) {
		printf("%02x ", frame.data[i]);
	}
	puts("");

out:
	ch9329_close(&ch9329);
	return 0;
}
