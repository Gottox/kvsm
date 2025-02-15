#include <SDL3/SDL.h>
#include <ch9329.h>
#include <stdbool.h>

#define INPUT_EVENT_CODE (Sint32)'i'
#define INPUT_EVENT_RING_SIZE 16
#define INPUT_KEY_DATA_SIZE 8
#define INPUT_MOUSE_DATA_SIZE 7

struct InputEvent {
	SDL_Event event;
	bool rel_mouse;
};

struct Input {
	SDL_Mutex *condition_mutex;
	SDL_Condition *condition;

	SDL_Thread *thread;

	SDL_Mutex *mutex;
	Uint64 status_interval;
	struct Ch9329 hid;
	struct Ch9329Frame hid_status;

	SDL_FRect rect;
	struct InputEvent event_ring[INPUT_EVENT_RING_SIZE];
	int event_ring_last_head;
	int event_ring_head;
	int event_ring_tail;
};

bool input_init(struct Input *input, const char *input_name);

bool input_start(struct Input *input);

bool input_status_numpad(struct Input *input);

bool input_status_capslock(struct Input *input);

bool input_status_scrolllock(struct Input *input);

bool input_status_connected(struct Input *input);

bool input_set_rect(struct Input *input, SDL_FRect *rect);

bool
input_send_input_event(struct Input *input, SDL_Event *event, bool rel_mouse);

bool input_cleanup(struct Input *input);
