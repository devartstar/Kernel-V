#include "assert.h"

typedef enum {
	PANIK_MODE_NORMAL,
	PANIK_MODE_TEST
} panik_mode_t;

void set_panik_state (panik_mode_t mode);
panik_mode_t get_panik_mode();

typedef struct {
	int panik_called;
	char last_panik_msg[256];
	int panik_call_count;
} panik_state_t;

void reset_panik_state(void);
const panik_state_t* get_panik_state();

void paink(cosnt char* fmt, ...) __attribute__((format(printf,1,2)));



