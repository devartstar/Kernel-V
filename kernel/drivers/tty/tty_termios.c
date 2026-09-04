#include "drivers/tty_termios.h"

void tty_termios_init_cooked(ktermios_t *t) {
    t->c_iflag = ICRNL;
    t->c_oflag = OPOST | ONLCR;
    t->c_lflag = ICANON | ECHO | ISIG;
    t->c_cc[VINTR] = 0x03;
    t->c_cc[VQUIT] = 0x1C;
}
