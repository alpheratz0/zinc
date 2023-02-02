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

extern void
pizarra_destroy(Pizarra *piz);
