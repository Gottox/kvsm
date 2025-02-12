#include <ch9329.h>

static int
serial_write(SerialPort port, const uint8_t *buf, int len) {
	return write(port, buf, len);
}

static int
serial_read(SerialPort port, uint8_t *buf, int len) {
	return read(port, buf, len);
}

static uint8_t
gen_checksum(const struct Ch9329Frame *frame) {
	uint8_t checksum = 0;
	const uint8_t len = ch9329_frame_len(frame);
	for (int i = 0; i < 5; i++) {
		checksum += frame->header[i];
	}
	for (int i = 0; i < len; i++) {
		checksum += frame->data[i];
	}
	return checksum;
}

int
ch9329_init(struct Ch9329 *ch9329, int fd) {
	int rv = 0;
	struct termios tio;

	ch9329->fd = fd;
	rv = tcgetattr(fd, &tio);
	if (rv < 0) {
		goto out;
	}
	memcpy(&ch9329->oldtio, &tio, sizeof(struct termios));

	// Configure 8-N-1 (8 data bits, no parity, 1 stop bit)
	tio.c_cflag &= ~CSIZE; // No parity
	tio.c_cflag &= ~PARENB; // No parity
	tio.c_cflag &= ~CSTOPB; // 1 stop bit
	tio.c_cflag |=
			CS8 | CREAD | CLOCAL; // Enable receiver, ignore modem control lines

	// Set raw mode (disable canonical mode, echo, signal chars)
	tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	// Disable software flow control
	tio.c_iflag &= ~(IXON | IXOFF | IXANY);

	// Disable special handling of bytes in output
	tio.c_oflag &= ~OPOST;

	rv = tcsetattr(fd, TCSANOW, &tio);
	if (rv < 0) {
		goto out;
	}

	rv = cfsetospeed(&tio, B115200);
	if (rv < 0) {
		goto out;
	}

out:
	return rv;
}

int
ch9329_open(struct Ch9329 *ch9329, const char *path) {
	int rv = 0;
	int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) {
		rv = fd;
		goto out;
	}

	rv = ch9329_init(ch9329, fd);
out:
	return rv;
}

int
ch9329_receive(struct Ch9329 *ch9329, struct Ch9329Frame *frame) {
	int rv = 0;
	uint8_t checksum = 0;

	// header
	rv = serial_read(ch9329->fd, frame->header, sizeof(frame->header));
	if (rv < (int)sizeof(frame->header)) {
		rv = -1;
		goto out;
	}

	uint8_t len = ch9329_frame_len(frame);
	if (len > 0) {
		// data
		rv = serial_read(ch9329->fd, frame->data, len);
		if (rv < len) {
			rv = -1;
			goto out;
		}
	}

	// checksum
	rv = serial_read(ch9329->fd, &checksum, 1);
	if (rv < 1) {
		rv = -1;
		goto out;
	}
	if (checksum != gen_checksum(frame)) {
		rv = -1;
		goto out;
	}

out:
	return rv;
}

int
ch9329_send(struct Ch9329 *ch9329, const struct Ch9329Frame *frame) {
	int rv = 0;
	uint8_t checksum = gen_checksum(frame);

	// header
	rv = serial_write(ch9329->fd, frame->header, sizeof(frame->header));
	if (rv < (int)sizeof(frame->header)) {
		rv = -1;
		goto out;
	}

	// data
	uint8_t len = ch9329_frame_len(frame);
	rv = serial_write(ch9329->fd, frame->data, len);
	if (rv < len) {
		rv = -1;
		goto out;
	}

	// checksum
	rv = serial_write(ch9329->fd, &checksum, 1);
	if (rv < 1) {
		rv = -1;
		goto out;
	}
out:
	return rv;
}

int
ch9329_close(struct Ch9329 *ch9329) {
	tcsetattr(ch9329->fd, TCSANOW, &ch9329->oldtio);
	close(ch9329->fd);
	return 0;
}
