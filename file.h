/*
 * @(#)$Id: file.h,v 2.0 2008/12/17 17:35:46 baccala Exp $
 *
 * Copyright (C) 1996 Tim Witham <twitham@pcocd2.intel.com>
 *
 * (see the files README and COPYING for more details)
 *
 * file variables and routines available outside of file.c
 *
 */

void
handle_opt(char opt, char *optarg);

void
writefile(char *filename);

void
readfile(char *filename);
