#pragma once

#include <xcb/xcb.h>
#include <xcb/xproto.h>

typedef struct Pizarra Pizarra;

extern Pizarra *
pizarra_new(xcb_connection_t *conn, xcb_window_t win);

extern void
pizarra_render(Pizarra *piz);

extern void
pizarra_camera_move_relative(Pizarra *piz, int offx, int offy);

extern void
pizarra_set_viewport(Pizarra *piz, int vw, int vh);

extern void
pizarra_set_pixel(Pizarra *piz, int x, int y, uint32_t color);

extern int
pizarra_get_pixel(Pizarra *piz, int x, int y, uint32_t *color);
