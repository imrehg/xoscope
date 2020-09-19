/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 1999 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * display variables and routines available outside of display.c
 *
 */

#define ALIGN_RIGHT     1
#define ALIGN_LEFT      2
#define ALIGN_CENTER    3
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

/* XXX fontname seems to be unused */
extern char fontname[];
extern char fonts[];
extern int total_horizontal_divisions;

void    init_widgets(void);
void    fix_widgets(void);

void    setup_help_text(GtkWidget *widget, gpointer ignored);
void    update_text(void);
void    show_data(void);
void    roundoff_multipliers(Channel *);
void    timebase_changed(void);
void    clear(void);
void    message(const char *);
void    animate(void *);

void    LoadSaveFile(int);
void    ExternCommand(void);
void    PerlFunction(void);
int     OpenDisplay(int, char **);

void    setup_help_text(GtkWidget *, gpointer);
