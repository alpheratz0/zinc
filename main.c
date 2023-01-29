#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_cursor.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "utils.h"
#include "chalkboard.h"

typedef struct {
	bool active;
	int x;
	int y;
} DraggingInfo;

static DraggingInfo dragging;
static struct chalkboard *chalkboard;
static xcb_connection_t *conn;
static xcb_screen_t *scr;
static xcb_window_t win;
static int width, height;
static xcb_key_symbols_t *ksyms;
static xcb_cursor_context_t *cctx;
static xcb_cursor_t cursor_hand, cursor_arrow;

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

extern void
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

	width = 800; height = 600;
	cursor_hand = xcb_cursor_load_cursor(cctx, "fleur");
	cursor_arrow = xcb_cursor_load_cursor(cctx, "left_ptr");
	ksyms = xcb_key_symbols_alloc(conn);
	win = xcb_generate_id(conn);

	xcb_create_window_aux(
		conn, scr->root_depth, win, scr->root, 0, 0,
		width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		scr->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
		(const xcb_create_window_value_list_t []) {{
			.background_pixel = 0x333333,
			.event_mask = XCB_EVENT_MASK_EXPOSURE |
			              XCB_EVENT_MASK_KEY_PRESS |
			              XCB_EVENT_MASK_KEY_RELEASE |
			              XCB_EVENT_MASK_BUTTON_PRESS |
			              XCB_EVENT_MASK_BUTTON_RELEASE |
			              XCB_EVENT_MASK_POINTER_MOTION |
			              XCB_EVENT_MASK_STRUCTURE_NOTIFY
		}}
	);

	_NET_WM_NAME = get_x11_atom("_NET_WM_NAME");
	UTF8_STRING = get_x11_atom("UTF8_STRING");
	wm_name = "xchalkboard";

	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
		_NET_WM_NAME, UTF8_STRING, 8, strlen(wm_name), wm_name);

	wm_class = "xchalkboard\0xchalkboard\0";
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

extern void
xwindestroy(void)
{
	xcb_free_cursor(conn, cursor_hand);
	xcb_free_cursor(conn, cursor_arrow);
	xcb_key_symbols_free(ksyms);
	xcb_destroy_window(conn, win);
	xcb_cursor_context_free(cctx);
	xcb_disconnect(conn);
}

static void
h_client_message(xcb_client_message_event_t *ev)
{
	xcb_atom_t WM_DELETE_WINDOW;

	WM_DELETE_WINDOW = get_x11_atom("WM_DELETE_WINDOW");

	/* check if the wm sent a delete window message */
	/* https://www.x.org/docs/ICCCM/icccm.pdf */
	if (ev->data.data32[0] == WM_DELETE_WINDOW) {
		xwindestroy();
		exit(0);
	}
}

static void
h_expose(xcb_expose_event_t *ev)
{
	(void) ev;
	chalkboard_render(conn, win, chalkboard);
}

static void
h_key_press(xcb_key_press_event_t *ev)
{
	xcb_keysym_t key;

	key = xcb_key_symbols_get_keysym(ksyms, ev->detail, 0);
}

static void
h_key_release(xcb_key_release_event_t *ev)
{
	xcb_keysym_t key;

	key = xcb_key_symbols_get_keysym(ksyms, ev->detail, 0);
}

static void
h_button_press(xcb_button_press_event_t *ev)
{
	switch (ev->detail) {
	case XCB_BUTTON_INDEX_1:
		chalkboard_set_pixel(chalkboard, ev->event_x, ev->event_y, 0xffffff);
		chalkboard_render(conn, win, chalkboard);
		break;
	case XCB_BUTTON_INDEX_2:
		dragging.active = true;
		dragging.x = ev->event_x;
		dragging.y = ev->event_y;
		break;
	}
	/* int32_t x, y; */
    /*  */
	/* switch (ev->detail) { */
	/* 	case XCB_BUTTON_INDEX_1: */
	/* 		if (!state.dragging && xwin2canvascoord(ev->event_x, ev->event_y, &x, &y)) { */
	/* 			state.painting = 1; */
	/* 			undohistorypush(); */
	/* 			xcanvasaddpoint(x, y, state.color, state.brush_size); */
	/* 		} */
	/* 		break; */
	/* 	case XCB_BUTTON_INDEX_2: */
	/* 		drag_begin(ev->event_x, ev->event_y); */
	/* 		break; */
	/* 	case XCB_BUTTON_INDEX_3: */
	/* 		if (xwin2canvascoord(ev->event_x, ev->event_y, &x, &y)) */
	/* 			set_color(canvas.px[y*canvas.width+x]); */
	/* 		break; */
	/* 	case XCB_BUTTON_INDEX_4: */
	/* 		set_brush_size(state.brush_size + 2); */
	/* 		break; */
	/* 	case XCB_BUTTON_INDEX_5: */
	/* 		set_brush_size(state.brush_size - 2); */
	/* 		break; */
	/* } */
}

static void
h_motion_notify(xcb_motion_notify_event_t *ev)
{
	int dx, dy;

	if (dragging.active) {
		dx = dragging.x - ev->event_x;
		dy = dragging.y - ev->event_y;

		dragging.x = ev->event_x;
		dragging.y = ev->event_y;

		chalkboard_move(chalkboard, dx, dy);
		chalkboard_render(conn, win, chalkboard);
	}
	/* int32_t x, y; */
    /*  */
	/* if (state.dragging) { */
	/* 	drag_update(ev->event_x, ev->event_y); */
	/* } else if (state.painting && xwin2canvascoord(ev->event_x, ev->event_y, &x, &y)) { */
	/* 	xcanvasaddpoint(x, y, state.color, state.brush_size); */
	/* } */
}

static void
h_button_release(xcb_button_release_event_t *ev)
{
	dragging.active = false;
	/* if (ev->detail == XCB_BUTTON_INDEX_1) { */
	/* 	state.painting = 0; */
	/* } else if (ev->detail == XCB_BUTTON_INDEX_2) { */
	/* 	drag_end(ev->event_x, ev->event_y); */
	/* } */
}

static void
h_configure_notify(xcb_configure_notify_event_t *ev)
{
	chalkboard_set_viewport(chalkboard, ev->width, ev->height);
}

static void
h_mapping_notify(xcb_mapping_notify_event_t *ev)
{
	if (ev->count > 0)
		xcb_refresh_keyboard_mapping(ksyms, ev);
}

/* static void */
/* usage(void) */
/* { */
/* 	puts("usage: apint [-fhv] [-l file] [-W width] [-H height]"); */
/* 	exit(0); */
/* } */
/*  */
/* static void */
/* version(void) */
/* { */
/* 	puts("apint version "VERSION); */
/* 	exit(0); */
/* } */

int
main(int argc, char **argv)
{
	enum chalkboard_direction cdir;
	xcb_generic_event_t *ev;

	cdir = DIRECTION_HORIZONTAL;

	while (++argv, --argc > 0) {
		if (!strcmp(*argv, "-horizontal")) cdir = DIRECTION_HORIZONTAL;
		else if (!strcmp(*argv, "-vertical")) cdir = DIRECTION_VERTICAL;
		else die("invalid option %s", *argv); break;
	}

	xwininit();

	chalkboard = chalkboard_new(conn, win, cdir);

	while ((ev = xcb_wait_for_event(conn))) {
		switch (ev->response_type & ~0x80) {
			case XCB_CLIENT_MESSAGE:     h_client_message((void *)(ev)); break;
			case XCB_EXPOSE:             h_expose((void *)(ev)); break;
			case XCB_KEY_PRESS:          h_key_press((void *)(ev)); break;
			case XCB_KEY_RELEASE:        h_key_release((void *)(ev)); break;
			case XCB_BUTTON_PRESS:       h_button_press((void *)(ev)); break;
			case XCB_MOTION_NOTIFY:      h_motion_notify((void *)(ev)); break;
			case XCB_BUTTON_RELEASE:     h_button_release((void *)(ev)); break;
			case XCB_CONFIGURE_NOTIFY:   h_configure_notify((void *)(ev)); break;
			case XCB_MAPPING_NOTIFY:     h_mapping_notify((void *)(ev)); break;
		}

		free(ev);
	}

	/* chalkboard_destroy(conn, win, chalkboard); */
	xwindestroy();

	return 0;
}

