#include <SDL3/SDL.h>
#include <jpeglib.h>
#include <stdbool.h>

#define CAMERA_EVENT_CODE (Sint32)'c'

struct Camera {
	SDL_TimerID timer_id;
	SDL_Mutex *mutex;
	SDL_Camera *camera;
	Uint64 timestamp;
	SDL_Surface *frame;
	SDL_Surface *jpeg_frame;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	SDL_CameraSpec spec;

	SDL_Texture *texture;
};

bool camera_init(struct Camera *camera, const char *camera_name);

bool camera_start(struct Camera *camera);

bool camera_size(struct Camera *camera, int *width, int *height);

bool camera_update_texture(struct Camera *camera, SDL_Renderer *renderer);

SDL_Texture *camera_texture(struct Camera *camera);

void camera_frame_release(struct Camera *camera, SDL_Surface *frame);

bool camera_cleanup(struct Camera *camera);
