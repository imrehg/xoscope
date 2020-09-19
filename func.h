/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 1997 Tim Witham <twitham@pcocd2.intel.com>
 *
 * (see the files README and COPYING for more details)
 *
 * xoscope math function definitions and prototypes
 *
 */

#define FUNCLEFT  0
#define FUNCRIGHT 1
#define FUNCPS    2
#define FUNCMEM   3
#define FUNCEXT   4
#define FUNC0     5

#define EXTSTOP  0
#define EXTSTART 1
#define EXTRUN   2

#if 0
#define FFT_TEST
#endif

struct signal_stats {
    short min;                  /* Minimum signal value */
    short max;                  /* Maximum signal value */
    int time;
    int freq;
#ifdef CALC_RMS
	double rms ;
#endif
};

void set_save_pending(char c);
void do_save_pending(void);
void save(int i, int src);
void recall_on_channel(Signal *, Channel *);
void recall(Signal *);

void next_func(void);
void prev_func(void);
int function_bynum_on_channel(int, Channel *);

void start_command_on_channel(const char *, Channel *);
void startcommand(const char *);
void start_perl_function(const char *);
void restart_external_commands(void);

void init_math(void);

int update_math_signals(void);

void do_math(void);

void cleanup_math(void);

void measure_data(Channel *, struct signal_stats *);


