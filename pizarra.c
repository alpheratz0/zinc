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

#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_image.h>
#include <xcb/shm.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include "pizarra.h"
#include "utils.h"

typedef struct {
	int x;
	int y;
} Vector2;

typedef struct Chunk {
	int index;
	int width;
	int height;
	uint32_t *px;
	char *stored_in;
	int loaded;

	/* X11 */
	int shm;
	xcb_gcontext_t gc;
	union {
		struct {
			int id;
			xcb_shm_seg_t seg;
			xcb_pixmap_t pixmap;
		} shm;
		xcb_image_t *image;
	} x;

	struct Chunk *next;
	struct Chunk *previous;
} Chunk;

struct Pizarra {
	Vector2 pos;
	int viewport_height;
	int viewport_width;

	Chunk *root;

	/* TODO */
	/* int min_visible_index; */
	/* int max_visible_index; */

	xcb_connection_t *conn;
	xcb_window_t win;
};

static int
__x_check_mit_shm_extension(xcb_connection_t *conn)
{
	xcb_generic_error_t *error;
	xcb_shm_query_version_cookie_t cookie;
	xcb_shm_query_version_reply_t *reply;

	cookie = xcb_shm_query_version(conn);
	reply = xcb_shm_query_version_reply(conn, cookie, &error);

	if (NULL != error) {
		if (NULL != reply)
			free(reply);
		free(error);
		return 0;
	}

	if (NULL != reply) {
		if (reply->shared_pixmaps == 0) {
			free(reply);
			return 0;
		}
		free(reply);
		return 1;
	}

	return 0;
}

static Chunk *
__chunk_first(const Chunk *c)
{
	Chunk *first;
	first = (Chunk *)c;
	while (first->previous)
		first = first->previous;
	return first;
}

static Chunk *
__chunk_last(const Chunk *c)
{
	Chunk *last;
	last = (Chunk *)c;
	while (last->next)
		last = last->next;
	return last;
}

static void
__pizarra_get_rect(const Pizarra *piz, int *x, int *y, int *w, int *h)
{
	const Chunk *first, *last;

	first = __chunk_first(piz->root);
	last = __chunk_last(piz->root);

	*x = 0;
	*y = first->index * piz->root->height;
	*w = piz->root->width;
	*h = (last->index - first->index + 1) * piz->root->height;
}

static void
__pizarra_get_viewport_rect(const Pizarra *piz, int *x, int *y, int *w, int *h)
{
	*x = piz->pos.x;
	*y = piz->pos.y;
	*w = piz->viewport_width;
	*h = piz->viewport_height;
}

static void
__chunk_get_rect(const Chunk *c, int *x, int *y, int *w, int *h)
{
	*x = 0;
	*y = c->index * c->height;
	*w = c->width;
	*h = c->height;
}

static Chunk *
__chunk_new(xcb_connection_t *conn, xcb_window_t win, int w, int h)
{
	Chunk *c;
	size_t szpx;
	xcb_screen_t *scr;
	uint8_t depth;

	assert(conn != NULL);
	assert(w > 0);
	assert(h > 0);

	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	assert(scr != NULL);

	szpx = w * h * sizeof(uint32_t);
	c = calloc(1, sizeof(Chunk));
	depth = scr->root_depth;

	c->width = w;
	c->height = h;

	c->gc = xcb_generate_id(conn);
	xcb_create_gc(conn, c->gc, win, 0, NULL);

	if (__x_check_mit_shm_extension(conn)) {
		c->shm = 1;

		c->x.shm.seg = xcb_generate_id(conn);
		c->x.shm.pixmap = xcb_generate_id(conn);
		c->x.shm.id = shmget(IPC_PRIVATE, szpx, IPC_CREAT | 0600);

		if (c->x.shm.id < 0)
			die("shmget failed");

		c->px = shmat(c->x.shm.id, NULL, 0);

		if (c->px == (void *) -1) {
			shmctl(c->x.shm.id, IPC_RMID, NULL);
			die("shmat failed");
		}

		xcb_shm_attach(conn, c->x.shm.seg, c->x.shm.id, 0);
		shmctl(c->x.shm.id, IPC_RMID, NULL);
		memset(c->px, 0, szpx);

		xcb_shm_create_pixmap(conn, c->x.shm.pixmap, win, w, h,
				depth, c->x.shm.seg, 0);
	} else {
		c->shm = 0;
		c->px = calloc(w * h, sizeof(uint32_t));

		if (NULL == c->px)
			die("OOM");

		c->x.image = xcb_image_create_native(conn, w, h,
				XCB_IMAGE_FORMAT_Z_PIXMAP, depth, c->px,
				szpx, (uint8_t *)(c->px));
	}

	return c;
}

static void
__chunk_prepend(xcb_connection_t *conn, xcb_window_t win, Chunk *c, int width, int height)
{
	Chunk *first;
	first = __chunk_first(c);
	first->previous = __chunk_new(conn, win, width, height);
	first->previous->next = first;
	first->previous->index = first->index - 1;
}

static void
__chunk_append(xcb_connection_t *conn, xcb_window_t win, Chunk *c, int width, int height)
{
	Chunk *last;
	last = __chunk_last(c);
	last->next = __chunk_new(conn, win, width, height);
	last->next->previous = last;
	last->next->index = last->index + 1;
}

extern Pizarra *
pizarra_new(xcb_connection_t *conn, xcb_window_t win)
{
	Pizarra *piz;
	xcb_screen_t *scr;
	int width, height;

	assert(conn != NULL);

	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	assert(scr != NULL);

	width = scr->width_in_pixels;
	height = scr->height_in_pixels;

	piz = calloc(1, sizeof(Pizarra));

	piz->conn = conn;
	piz->win = win;
	piz->root = __chunk_new(conn, win, width, height);
	piz->root->index = 0;

	__chunk_prepend(conn, win, piz->root, width, height);
	__chunk_append(conn, win, piz->root, width, height);

	return piz;
}

static void
pizarra_regenerate_chunks(Pizarra *piz)
{
	int x, y, w, h;
	int cx, cy, cw, ch;
	bool done;

regenerate:
	done = true;
	__pizarra_get_viewport_rect(piz, &x, &y, &w, &h);
	__pizarra_get_rect(piz, &cx, &cy, &cw, &ch);

	if (y < cy) {
		__chunk_prepend(piz->conn, piz->win, __chunk_first(piz->root),
				piz->root->width, piz->root->height);
		done = false;
	}

	if (y + h > cy + ch) {
		__chunk_append(piz->conn, piz->win, __chunk_last(piz->root),
				piz->root->width, piz->root->height);
		done = false;
	}

	if (!done)
		goto regenerate;
}

extern void
pizarra_camera_move_relative(Pizarra *piz, int offx, int offy)
{
	piz->pos.x += offx;
	piz->pos.y += offy;

	pizarra_regenerate_chunks(piz);

	if (piz->pos.x > piz->root->width)
		piz->pos.x = piz->root->width;

	if (piz->pos.x < -piz->viewport_width)
		piz->pos.x = -piz->viewport_width;
}

extern void
pizarra_set_viewport(Pizarra *piz, int vw, int vh)
{
	piz->viewport_width = vw;
	piz->viewport_height = vh;

	pizarra_regenerate_chunks(piz);

	if (piz->pos.x > piz->root->width)
		piz->pos.x = piz->root->width;

	if (piz->pos.x < -piz->viewport_width)
		piz->pos.x = -piz->viewport_width;
}

extern void
pizarra_render(Pizarra *piz)
{
	int x, y, w, h;
	int cx, cy, cw, ch;
	Chunk *chunk;

	__pizarra_get_viewport_rect(piz, &x, &y, &w, &h);

	if (piz->pos.x < 0)
		xcb_clear_area(piz->conn, 0, piz->win, 0, 0, -piz->pos.x, piz->viewport_height);

	if (piz->root->width - piz->pos.x < piz->viewport_width)
		xcb_clear_area(piz->conn, 0, piz->win, piz->root->width - piz->pos.x,
				0, piz->viewport_width - (piz->root->width - piz->pos.x),
				piz->viewport_height);

	for (chunk = __chunk_first(piz->root); chunk; chunk = chunk->next) {
		__chunk_get_rect(chunk, &cx, &cy, &cw, &ch);

		if (x < cx + cw && y < cy + ch
				&& x + w > cx && y + h > cy) {
			if (chunk->shm) {
				xcb_copy_area(piz->conn, chunk->x.shm.pixmap, piz->win,
						chunk->gc, 0, 0, cx - piz->pos.x, cy - piz->pos.y, cw, ch);
			} else {
				xcb_image_put(piz->conn, piz->win, chunk->gc,
						chunk->x.image, cx - piz->pos.x, cy - piz->pos.y, 0);
			}
		}
	}

	xcb_flush(piz->conn);
}

extern void
pizarra_set_pixel(Pizarra *piz, int x, int y, uint32_t color)
{
	int chunkx, chunky;
	Chunk *chunk;

	x += piz->pos.x;
	y += piz->pos.y;

	for (chunk = __chunk_first(piz->root); chunk; chunk = chunk->next) {
		chunkx = 0;
		chunky = chunk->index * chunk->height;

		if (x >= chunkx && x < chunkx + chunk->width
				&& y >= chunky && y < chunky + chunk->height) {
			chunk->px[(y-chunky)*chunk->width+(x-chunkx)] = color;
			break;
		}
	}
}

extern int
pizarra_get_pixel(Pizarra *piz, int x, int y, uint32_t *color)
{
	int chunkx, chunky;
	Chunk *chunk;

	x += piz->pos.x;
	y += piz->pos.y;

	for (chunk = __chunk_first(piz->root); chunk; chunk = chunk->next) {
		chunkx = 0;
		chunky = chunk->index * chunk->height;

		if (x >= chunkx && x < chunkx + chunk->width
				&& y >= chunky && y < chunky + chunk->height) {
			*color = chunk->px[(y-chunky)*chunk->width+(x-chunkx)];
			return 1;
		}
	}

	return 0;
}
