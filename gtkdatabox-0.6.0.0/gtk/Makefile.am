
noinst_LIBRARIES		= libgtkdatabox.a
libgtkdatabox_a_SOURCES= \
			gtkdatabox.c\
			gtkdatabox_marshal.c\
			gtkdatabox_xyc_graph.c\
			gtkdatabox_graph.c\
			gtkdatabox_points.c\
                        gtkdatabox_lines.c\
                        gtkdatabox_bars.c\
                        gtkdatabox_grid.c\
                        gtkdatabox_cross_simple.c\
			gtkdatabox_marker.c

noinst_HEADERS		= \
			gtkdatabox.h\
			gtkdatabox_marshal.h\
			gtkdatabox_graph.h\
			gtkdatabox_xyc_graph.h\
			gtkdatabox_points.h\
                        gtkdatabox_lines.h\
                        gtkdatabox_bars.h\
                        gtkdatabox_grid.h\
                        gtkdatabox_cross_simple.h\
			gtkdatabox_marker.h

INCLUDES		= \
			-I$(top_srcdir)

AM_CFLAGS			=\
			-O2\
			-Wall -pedantic\
			-DG_DISABLE_DEPRECATED\
			-DGDK_DISABLE_DEPRECATED\
			-DGDK_PIXBUF_DISABLE_DEPRECATED\
			-DGTK_DISABLE_DEPRECATED\
			`pkg-config gtk+-2.0 --cflags`
