#include "camera.h"

#include <assert.h>
#include <stdbool.h>

static bool
decode_frame(
		struct jpeg_decompress_struct *cinfo, SDL_Surface *source,
		SDL_Surface *target) {
	JSAMPARRAY buffer;
	int row_stride;

	jpeg_mem_src(cinfo, (unsigned char *)source->pixels, source->pitch);
	if (jpeg_read_header(cinfo, TRUE) != JPEG_HEADER_OK) {
		jpeg_abort_decompress(cinfo);
		return false;
	}

	jpeg_start_decompress(cinfo);

	if (target->w != (int)cinfo->output_width ||
		target->h != (int)cinfo->output_height) {
		SDL_Log("Target surface size does not match JPEG size");
		jpeg_abort_decompress(cinfo);
		return false;
	}

	row_stride = cinfo->output_width * cinfo->output_components;
	buffer = (*cinfo->mem->alloc_sarray)(
			(j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);

	while (cinfo->output_scanline < cinfo->output_height) {
		jpeg_read_scanlines(cinfo, buffer, 1);
		memcpy((Uint8 *)target->pixels +
					   cinfo->output_scanline * target->pitch - target->pitch,
			   buffer[0], row_stride);
	}

	jpeg_finish_decompress(cinfo);

	return true;
}

static bool
update_camera_frame(struct Camera *camera) {
	bool rv = false;
	Uint64 frame_timestamp = 0;
	SDL_Surface *jpeg_frame =
			SDL_AcquireCameraFrame(camera->camera, &frame_timestamp);

	SDL_LockMutex(camera->mutex);

	if (!jpeg_frame || frame_timestamp == camera->timestamp) {
		goto out;
	}
	camera->timestamp = frame_timestamp;

	if (camera->jpeg_frame &&
		memcmp(jpeg_frame->pixels, camera->jpeg_frame->pixels,
			   jpeg_frame->pitch) == 0) {
		goto out;
	}

	if (!camera->frame) {
		camera->frame = SDL_CreateSurface(
				jpeg_frame->w, jpeg_frame->h, SDL_PIXELFORMAT_RGB24);
		if (!camera->frame) {
			SDL_Log("Failed to create frame surface");
			goto out;
		}
	}

	if (!decode_frame(&camera->cinfo, jpeg_frame, camera->frame)) {
		SDL_Log("Failed to decode JPEG to texture");
		goto out;
	}

	rv = true;
out:
	SDL_ReleaseCameraFrame(camera->camera, camera->jpeg_frame);
	camera->jpeg_frame = jpeg_frame;
	SDL_UnlockMutex(camera->mutex);
	return rv;
}

static Uint32
update_camera_frame_timer(void *userdata, SDL_TimerID id, Uint32 interval) {
	(void)id;

	struct Camera *ui = (struct Camera *)userdata;

	if (!update_camera_frame(ui)) {
		return interval;
	}

	SDL_Event event = {
			.user = {
					.type = SDL_EVENT_USER,
					.code = CAMERA_EVENT_CODE,
			}};
	SDL_PushEvent(&event);
	return interval;
}

bool
camera_init(struct Camera *camera, const char *camera_name) {
	int devcount = 0;
	SDL_CameraID *devices = SDL_GetCameras(&devcount);
	SDL_CameraID target_device = 0;
	for (int i = 0; i < devcount; i++) {
		if (strcmp(camera_name, SDL_GetCameraName(devices[i])) == 0) {
			target_device = devices[i];
			break;
		}
	}
	SDL_free(devices);

	const SDL_CameraSpec desired_spec = {
			.format = SDL_PIXELFORMAT_MJPG,
			.width = 0,
			.height = 0,
			.framerate_numerator = 30,
			.framerate_denominator = 1,
	};
	camera->camera = SDL_OpenCamera(target_device, &desired_spec);
	if (!camera->camera) {
		SDL_Log("Couldn't open camera: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	if (!SDL_GetCameraFormat(camera->camera, &camera->spec)) {
		SDL_Log("Couldn't get camera spec: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	SDL_LogTrace(
			SDL_LOG_CATEGORY_APPLICATION, "Camera spec: %dx%d %d/%d %d\n",
			camera->spec.width, camera->spec.height,
			camera->spec.framerate_numerator,
			camera->spec.framerate_denominator,
			camera->spec.format == SDL_PIXELFORMAT_MJPG);
	camera->mutex = SDL_CreateMutex();

	camera->cinfo.err = jpeg_std_error(&camera->jerr);

	jpeg_create_decompress(&camera->cinfo);

	return true;
}

bool
camera_size(struct Camera *camera, int *w, int *h) {
	bool rv = false;
	SDL_LockMutex(camera->mutex);
	if (camera->frame) {
		*w = camera->frame->w;
		*h = camera->frame->h;
		rv = true;
	}
	SDL_UnlockMutex(camera->mutex);
	return rv;
}

bool
camera_start(struct Camera *camera) {
	if (camera->timer_id) {
		SDL_Log("Camera already started");
		return false;
	}

	const int frame_time = camera->spec.framerate_denominator * 1000 /
			camera->spec.framerate_numerator;

	camera->timer_id =
			SDL_AddTimer(frame_time, update_camera_frame_timer, camera);
	if (!camera->timer_id) {
		SDL_Log("Failed to create camera timer: %s", SDL_GetError());
		return false;
	}
	return true;
}

bool
camera_update_texture(struct Camera *camera, SDL_Renderer *renderer) {
	bool rv = false;
	if (renderer == NULL) {
		return rv;
	}

	SDL_LockMutex(camera->mutex);
	if (!camera->frame) {
		goto out;
	}
	if (!camera->texture) {
		SDL_LogTrace(SDL_LOG_CATEGORY_RENDER, "Creating window texture");
		camera->texture = SDL_CreateTexture(
				renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
				camera->frame->w, camera->frame->h);
		if (!camera->texture) {
			SDL_Log("Failed to create camera texture: %s", SDL_GetError());
			goto out;
		}
	}
	if (!SDL_UpdateTexture(
				camera->texture, NULL, camera->frame->pixels,
				camera->frame->pitch)) {
		SDL_Log("Failed to update camera texture: %s", SDL_GetError());
		goto out;
	}

	rv = true;
out:
	SDL_UnlockMutex(camera->mutex);
	return rv;
}

struct SDL_Texture *
camera_texture(struct Camera *camera) {
	return camera->texture;
}

bool
camera_cleanup(struct Camera *camera) {
	SDL_RemoveTimer(camera->timer_id);
	SDL_DestroyTexture(camera->texture);
	SDL_CloseCamera(camera->camera);
	SDL_DestroySurface(camera->frame);
	SDL_DestroyMutex(camera->mutex);
	jpeg_destroy_decompress(&camera->cinfo);
	return true;
}
