#ifndef BSRSETUP_COLORS_H
#define BSRSETUP_COLORS_H

#include <bsr.h>

enum when_color { NEVER_COLOR = -1, AUTO_COLOR = 0, ALWAYS_COLOR = 1 };
extern enum when_color opt_color;

extern const char *stop_color_code(void);
extern const char *role_color_start(enum bsr_role, bool);
extern const char *role_color_stop(enum bsr_role, bool);
extern const char *cstate_color_start(enum bsr_conn_state);
extern const char *cstate_color_stop(enum bsr_conn_state);
extern const char *repl_state_color_start(enum bsr_repl_state);
extern const char *repl_state_color_stop(enum bsr_repl_state);
extern const char *disk_state_color_start(enum bsr_disk_state, bool intentional, bool);
extern const char *disk_state_color_stop(enum bsr_disk_state, bool);

// DW-1755
extern const char *io_error_color_start();
extern const char *io_error_color_stop();

#define REPL_COLOR_STRING(__r)  \
	repl_state_color_start(__r), bsr_repl_str(__r), repl_state_color_stop(__r)

#define DISK_COLOR_STRING(__d, __intentional, __local) \
    disk_state_color_start(__d, __intentional, __local), bsr_disk_str(__d), disk_state_color_stop(__d, __local)

#define ROLE_COLOR_STRING(__r, __local) \
	role_color_start(__r, __local), bsr_role_str(__r), role_color_stop(__r, __local)

#define CONN_COLOR_STRING(__c) \
	cstate_color_start(__c), bsr_conn_str(__c), cstate_color_stop(__c)

#endif  /* BSRSETUP_COLORS_H */
