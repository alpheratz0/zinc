#pragma once

#include <xcb/xcb.h>
#include <xcb/xproto.h>

struct pizarra;

extern struct pizarra *
pizarra_new(xcb_connection_t *conn, xcb_window_t win);

extern void
pizarra_render(xcb_connection_t *conn, xcb_window_t win, struct pizarra *c);

extern void
pizarra_move(xcb_connection_t *conn, xcb_window_t win, struct pizarra *c, int offx, int offy);

extern void
pizarra_set_viewport(xcb_connection_t *conn, xcb_window_t win, struct pizarra *c, int vw, int vh);

extern void
pizarra_set_pixel(struct pizarra *c, int x, int y, uint32_t color);

extern int
pizarra_get_pixel(struct pizarra *c, int x, int y, uint32_t *color);
