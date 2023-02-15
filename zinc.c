/*
	Copyright (C) 2023 <alpheratz99@protonmail.com>

	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public License version 2 as published by the
	Free Software Foundation.

	This program is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place, Suite 330, Boston, MA 02111-1307 USA

*/

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_cursor.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "utils.h"
#include "pizarra.h"
#include "history.h"

typedef struct {
	bool active;
	int x;
	int y;
} DragInfo;

typedef struct {
	bool active;
	uint32_t color;
	int brush_size;
} DrawInfo;

static Pizarra *pizarra;
static History *hist;
static HistoryUserAction *hist_last_action;
static xcb_connection_t *conn;
static xcb_screen_t *scr;
static xcb_window_t win;
static xcb_key_symbols_t *ksyms;
static xcb_cursor_context_t *cctx;
static xcb_cursor_t cursor_hand, cursor_arrow;
static DrawInfo drawinfo;
static DragInfo draginfo;
static bool should_close;

static xcb_atom_t
get_x11_atom(const char *name)
{
	xcb_atom_t atom;
	xcb_generic_error_t *error;
	xcb_intern_atom_cookie_t cookie;
	xcb_intern_atom_reply_t *reply;

	cookie = xcb_intern_atom(conn, 0, strlen(name), name);
	reply = xcb_intern_atom_reply(conn, cookie, &error);

	if (NULL != error)
		die("xcb_intern_atom failed with error code: %hhu",
				error->error_code);

	atom = reply->atom;
	free(reply);

	return atom;
}

static void
xwininit(void)
{
	const char *wm_class,
		       *wm_name;

	xcb_atom_t _NET_WM_NAME,
			   _NET_WM_STATE,
			   _NET_WM_STATE_FULLSCREEN,
			   _NET_WM_WINDOW_OPACITY;

	xcb_atom_t WM_PROTOCOLS,
			   WM_DELETE_WINDOW;

	xcb_atom_t UTF8_STRING;

	uint8_t opacity[4];

	conn = xcb_connect(NULL, NULL);

	if (xcb_connection_has_error(conn))
		die("can't open display");

	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	if (NULL == scr)
		die("can't get default screen");

	if (xcb_cursor_context_new(conn, scr, &cctx) != 0)
		die("can't create cursor context");

	cursor_hand = xcb_cursor_load_cursor(cctx, "fleur");
	cursor_arrow = xcb_cursor_load_cursor(cctx, "left_ptr");
	ksyms = xcb_key_symbols_alloc(conn);
	win = xcb_generate_id(conn);

	xcb_create_window_aux(
		conn, scr->root_depth, win, scr->root, 0, 0,
		800, 600, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		scr->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
		(const xcb_create_window_value_list_t []) {{
			.background_pixel = 0x0e0e0e,
			.event_mask = XCB_EVENT_MASK_EXPOSURE |
			              XCB_EVENT_MASK_KEY_PRESS |
			              XCB_EVENT_MASK_BUTTON_PRESS |
			              XCB_EVENT_MASK_BUTTON_RELEASE |
			              XCB_EVENT_MASK_POINTER_MOTION |
			              XCB_EVENT_MASK_STRUCTURE_NOTIFY
		}}
	);

	_NET_WM_NAME = get_x11_atom("_NET_WM_NAME");
	UTF8_STRING = get_x11_atom("UTF8_STRING");
	wm_name = "zinc";

	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
		_NET_WM_NAME, UTF8_STRING, 8, strlen(wm_name), wm_name);

	wm_class = "zinc\0zinc\0";
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, XCB_ATOM_WM_CLASS,
		XCB_ATOM_STRING, 8, strlen(wm_class), wm_class);

	WM_PROTOCOLS = get_x11_atom("WM_PROTOCOLS");
	WM_DELETE_WINDOW = get_x11_atom("WM_DELETE_WINDOW");

	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
		WM_PROTOCOLS, XCB_ATOM_ATOM, 32, 1, &WM_DELETE_WINDOW);

	_NET_WM_WINDOW_OPACITY = get_x11_atom("_NET_WM_WINDOW_OPACITY");
	opacity[0] = opacity[1] = opacity[2] = opacity[3] = 0xff;

	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
		_NET_WM_WINDOW_OPACITY, XCB_ATOM_CARDINAL, 32, 1, opacity);

	_NET_WM_STATE = get_x11_atom("_NET_WM_STATE");
	_NET_WM_STATE_FULLSCREEN = get_x11_atom("_NET_WM_STATE_FULLSCREEN");
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
		_NET_WM_STATE, XCB_ATOM_ATOM, 32, 1, &_NET_WM_STATE_FULLSCREEN);

	xcb_map_window(conn, win);
	xcb_flush(conn);
}

static void
xwindestroy(void)
{
	xcb_free_cursor(conn, cursor_hand);
	xcb_free_cursor(conn, cursor_arrow);
	xcb_key_symbols_free(ksyms);
	xcb_destroy_window(conn, win);
	xcb_cursor_context_free(cctx);
	xcb_disconnect(conn);
}

#ifndef ZINC_USE_ROUGH_BRUSH
static inline uint8_t
blerp(uint8_t from, uint8_t to, double v)
{
	return from + ((to - from) * v);
}

static uint32_t
color_lerp(uint32_t from, uint32_t to, double v)
{
	uint8_t r, g, b;

	v = v > 1 ? 1 : v < 0 ? 0 : v;
	r = blerp((from >> 16) & 0xff, (to >> 16) & 0xff, v);
	g = blerp((from >> 8) & 0xff, (to >> 8) & 0xff, v);
	b = blerp(from & 0xff, to & 0xff, v);

	return (r << 16) | (g << 8) | b;
}
#endif

static void
addpoint(int x, int y, uint32_t color, int size, bool add_to_history)
{
	int canvasx, canvasy;
	uint32_t prevcol;
	int dx, dy;

	if (add_to_history) {
		if (NULL == hist_last_action)
			hist_last_action = history_user_action_new();
		pizarra_camera_to_canvas_pos(pizarra, x, y, &canvasx, &canvasy);
		history_user_action_push_atomic(hist_last_action,
				history_atomic_action_add_point_new(canvasx, canvasy,
					color, size));
	}

	for (dy = -size; dy < size; ++dy) {
		for (dx = -size; dx < size; ++dx) {
			if (dy * dy + dx * dx >= size * size ||
					!pizarra_get_pixel(pizarra, x + dx, y + dy, &prevcol))
				continue;
#ifdef ZINC_USE_ROUGH_BRUSH
			pizarra_set_pixel(pizarra, x + dx, y + dy, color);
#else
			pizarra_set_pixel(pizarra, x + dx, y + dy,
					color_lerp(color, prevcol,
						sqrt(dy * dy + dx * dx) / size));
#endif
		}
	}
}

static void
h_client_message(xcb_client_message_event_t *ev)
{
	xcb_atom_t WM_DELETE_WINDOW;

	WM_DELETE_WINDOW = get_x11_atom("WM_DELETE_WINDOW");

	/* check if the wm sent a delete window message */
	/* https://www.x.org/docs/ICCCM/icccm.pdf */
	if (ev->data.data32[0] == WM_DELETE_WINDOW)
		should_close = true;
}

static void
h_expose(xcb_expose_event_t *ev)
{
	(void) ev;
	pizarra_render(pizarra);
}

static void
h_key_press(xcb_key_press_event_t *ev)
{
	int x, y;
	size_t i, j;
	HistoryAtomicAction *act;
	xcb_keysym_t key;

	key = xcb_key_symbols_get_keysym(ksyms, ev->detail, 0);

	switch (key) {
	case XKB_KEY_r: drawinfo.color = 0xb81c00; break; /* Red */
	case XKB_KEY_g: drawinfo.color = 0x50c878; break; /* Green */
	case XKB_KEY_b: drawinfo.color = 0x1239e6; break; /* Blue */
	case XKB_KEY_w: drawinfo.color = 0xffffff; break; /* White */
	case XKB_KEY_q: drawinfo.color = 0x000000; break; /* Black */
	case XKB_KEY_o: drawinfo.color = 0xcc551f; break; /* Orange */
	case XKB_KEY_y: drawinfo.color = 0xffff00; break; /* Yellow */
	case XKB_KEY_f: drawinfo.color = 0xca2c92; break; /* Fuchsia */
	case XKB_KEY_t: drawinfo.color = 0x008080; break; /* Teal */
	case XKB_KEY_c: drawinfo.color = 0xfffdd0; break; /* Cream */
	}

	if (key == XKB_KEY_z && ev->state & XCB_MOD_MASK_CONTROL && !drawinfo.active) {
		history_undo(hist);
		pizarra_clear(pizarra);

		for (i = 0; i < hist->len; ++i) {
			for (j = 0; j < hist->actions[i]->n; ++j) {
				act = hist->actions[i]->aa[j];
				if (act->kind == HISTORY_ADD_POINT) {
					pizarra_canvas_to_camera_pos(pizarra, act->info.ap.x, act->info.ap.y, &x, &y);
					addpoint(x, y, act->info.ap.color, act->info.ap.size, false);
				}
			}
		}

		pizarra_render(pizarra);
	}
}

static void
h_button_press(xcb_button_press_event_t *ev)
{
	switch (ev->detail) {
	case XCB_BUTTON_INDEX_1:
		if (draginfo.active)
			break;
		drawinfo.active = true;
		addpoint(ev->event_x, ev->event_y, drawinfo.color, drawinfo.brush_size, true);
		pizarra_render(pizarra);
		break;
	case XCB_BUTTON_INDEX_2:
		if (drawinfo.active)
			break;
		draginfo.active = true;
		draginfo.x = ev->event_x;
		draginfo.y = ev->event_y;
		xcb_change_window_attributes(conn, win, XCB_CW_CURSOR, &cursor_hand);
		xcb_flush(conn);
		break;
	case XCB_BUTTON_INDEX_4:
		pizarra_camera_move_relative(pizarra, 0, -30);
		pizarra_render(pizarra);
		break;
	case XCB_BUTTON_INDEX_5:
		pizarra_camera_move_relative(pizarra, 0, 30);
		pizarra_render(pizarra);
		break;
	}
}

static void
h_motion_notify(xcb_motion_notify_event_t *ev)
{
	int dx, dy;

	if (draginfo.active) {
		dx = draginfo.x - ev->event_x;
		dy = draginfo.y - ev->event_y;

		draginfo.x = ev->event_x;
		draginfo.y = ev->event_y;

		pizarra_camera_move_relative(pizarra, dx, dy);
		pizarra_render(pizarra);
	}

	if (drawinfo.active) {
		addpoint(ev->event_x, ev->event_y, drawinfo.color, drawinfo.brush_size, true);
		pizarra_render(pizarra);
	}
}

static void
h_button_release(xcb_button_release_event_t *ev)
{
	switch (ev->detail) {
	case XCB_BUTTON_INDEX_1:
		drawinfo.active = false;
		if (NULL == hist_last_action)
			break;
		history_do(hist, hist_last_action);
		hist_last_action = NULL;
		break;
	case XCB_BUTTON_INDEX_2:
		draginfo.active = false;
		xcb_change_window_attributes(conn, win, XCB_CW_CURSOR, &cursor_arrow);
		xcb_flush(conn);
		break;
	}
}

static void
h_configure_notify(xcb_configure_notify_event_t *ev)
{
	pizarra_set_viewport(pizarra, ev->width, ev->height);
}

static void
h_mapping_notify(xcb_mapping_notify_event_t *ev)
{
	if (ev->count > 0)
		xcb_refresh_keyboard_mapping(ksyms, ev);
}

static void
usage(void)
{
	puts("usage: zinc [-hv]");
	exit(0);
}

static void
version(void)
{
	puts("zinc version "VERSION);
	exit(0);
}

int
main(int argc, char **argv)
{
	xcb_generic_event_t *ev;

	while (++argv, --argc > 0) {
		if ((*argv)[0] == '-' && (*argv)[1] != '\0' && (*argv)[2] == '\0') {
			switch ((*argv)[1]) {
			case 'h': usage(); break;
			case 'v': version(); break;
			default: die("invalid option %s", *argv); break;
			}
		} else {
			die("unexpected argument: %s", *argv);
		}
	}

	xwininit();

	drawinfo.color = 0xffffff;
	drawinfo.brush_size = 5;

	pizarra = pizarra_new(conn, win);
	hist = history_new();

	while (!should_close && (ev = xcb_wait_for_event(conn))) {
		switch (ev->response_type & ~0x80) {
		case XCB_CLIENT_MESSAGE:     h_client_message((void *)(ev)); break;
		case XCB_EXPOSE:             h_expose((void *)(ev)); break;
		case XCB_KEY_PRESS:          h_key_press((void *)(ev)); break;
		case XCB_BUTTON_PRESS:       h_button_press((void *)(ev)); break;
		case XCB_MOTION_NOTIFY:      h_motion_notify((void *)(ev)); break;
		case XCB_BUTTON_RELEASE:     h_button_release((void *)(ev)); break;
		case XCB_CONFIGURE_NOTIFY:   h_configure_notify((void *)(ev)); break;
		case XCB_MAPPING_NOTIFY:     h_mapping_notify((void *)(ev)); break;
		}

		free(ev);
	}

	history_destroy(hist);
	pizarra_destroy(pizarra);
	xwindestroy();

	return 0;
}
