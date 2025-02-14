#include <ch9329.h>
#include <string.h>
#include <sys/select.h>

static int
serial_write(SerialPort port, const uint8_t *buf, int len) {
	return write(port, buf, len);
}

static int
serial_read(SerialPort port, uint8_t *buf, int len) {
	return read(port, buf, len);
}

static int
serial_wait(struct Ch9329 *ch9329, int timeout) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(ch9329->fd, &fds);
	struct timeval tv = {0};
	tv.tv_usec = timeout * 1000;
	tv.tv_sec = timeout / 1000;
	return select(ch9329->fd + 1, &fds, NULL, NULL, &tv) > 0;
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
ch9329_init(struct Ch9329 *ch9329, int fd, int timeout) {
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

	ch9329->timeout = timeout;
out:
	return rv;
}

int
ch9329_open(struct Ch9329 *ch9329, const char *path, int timeout) {
	int rv = 0;
	int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) {
		rv = fd;
		goto out;
	}

	rv = ch9329_init(ch9329, fd, timeout);
out:
	return rv;
}

int
ch9329_receive(struct Ch9329 *ch9329, struct Ch9329Frame *frame) {
	int rv = 0;
	uint8_t checksum = 0;
	memset(frame, 0, sizeof(struct Ch9329Frame));

	// header
	if (ch9329->timeout > 0) {
		rv = serial_wait(ch9329, ch9329->timeout);
		if (rv < 0) {
			goto out;
		}
	}
	rv = serial_read(ch9329->fd, frame->header, sizeof(frame->header));
	if (rv < (int)sizeof(frame->header)) {
		rv = -1;
		goto out;
	}

	// data
	uint8_t len = ch9329_frame_len(frame);
	if (len > 0) {
		if (ch9329->timeout > 0) {
			rv = serial_wait(ch9329, ch9329->timeout);
			if (rv < 0) {
				goto out;
			}
		}
		rv = serial_read(ch9329->fd, frame->data, len);
		if (rv < len) {
			rv = -1;
			goto out;
		}
	}

	// checksum
	if (ch9329->timeout > 0) {
		rv = serial_wait(ch9329, ch9329->timeout);
		if (rv < 0) {
			goto out;
		}
	}
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
ch9329_request(struct Ch9329 *ch9329, struct Ch9329Frame *frame) {
	int rv = 0;
	enum Ch9329Command command = ch9329_frame_command(frame);

	rv = ch9329_send(ch9329, frame);
	if (rv < 0) {
		goto out;
	}

	rv = ch9329_receive(ch9329, frame);
	if (rv < 0) {
		goto out;
	}

	rv = -ch9329_frame_error(frame);
	if (rv < 0) {
		goto out;
	}

	if (ch9329_frame_command(frame) != (0x80 | command)) {
		rv = -1;
		goto out;
	}

out:
	return rv;
}

int
ch9329_reset(struct Ch9329 *ch9329) {
	struct Ch9329Frame frame = {0};
	ch9329_frame(&frame, CH9329_CMD_RESET, NULL, 0);
	return ch9329_request(ch9329, &frame);
}

int
ch9329_close(struct Ch9329 *ch9329) {
	tcsetattr(ch9329->fd, TCSANOW, &ch9329->oldtio);
	close(ch9329->fd);
	return 0;
}
