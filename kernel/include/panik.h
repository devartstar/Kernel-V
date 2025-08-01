#include "assert.h"

typedef enum {
	PANIK_MODE_NORMAL,
	PANIK_MODE_TEST
} panik_mode_t;

void set_panik_mode(panik_mode_t mode);
panik_mode_t get_panik_mode(void);

typedef struct {
	int panik_called;
	char last_panik_msg[256];
	int panik_call_count;
} panik_state_t;

void reset_panik_state(void);
const panik_state_t* get_panik_state(void);

void panik(const char* fmt, ...) __attribute__((format(printf,1,2))) __attribute__((noreturn));



