#pragma once

#include <xcb/xcb.h>
#include <xcb/xproto.h>

struct chalkboard;

enum chalkboard_direction {
	DIRECTION_HORIZONTAL,
	DIRECTION_VERTICAL
};

extern struct chalkboard *
chalkboard_new(xcb_connection_t *conn, xcb_window_t win, enum chalkboard_direction dir);

extern void
chalkboard_render(xcb_connection_t *conn, xcb_window_t win, struct chalkboard *c);
