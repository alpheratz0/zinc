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

#include "chalkboard.h"
#include "utils.h"

struct vec2 {
	int x;
	int y;
};

struct chunk {
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

	struct chunk *next;
	struct chunk *previous;
};

struct chalkboard {
	struct vec2 pos;
	int viewport_height;
	int viewport_width;
	enum chalkboard_direction dir;
	struct chunk *root;
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

static struct chunk *
__chunk_new(xcb_connection_t *conn, xcb_window_t win, int w, int h)
{
	struct chunk *c;
	size_t szpx;
	xcb_screen_t *scr;
	uint8_t depth;

	assert(conn != NULL);
	assert(w > 0);
	assert(h > 0);

	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	assert(scr != NULL);

	szpx = w * h * sizeof(uint32_t);
	c = calloc(1, sizeof(struct chunk));
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

static struct chunk *
__chunk_first(const struct chunk *c)
{
	struct chunk *first;
	first = c;
	while (first->previous)
		first = first->previous;
	return first;
}

static struct chunk *
__chunk_last(const struct chunk *c)
{
	struct chunk *last;
	last = c;
	while (last->next)
		last = last->next;
	return last;
}

static void
__chunk_prepend(xcb_connection_t *conn, xcb_window_t win, struct chunk *c, int width, int height)
{
	struct chunk *first;
	first = __chunk_first(c);
	first->previous = __chunk_new(conn, win, width, height);
	first->previous->next = first;
	first->previous->index = first->index - 1;
}

static void
__chunk_apend(xcb_connection_t *conn, xcb_window_t win, struct chunk *c, int width, int height)
{
	struct chunk *last;
	last = __chunk_last(c);
	last->next = __chunk_new(conn, win, width, height);
	last->next->previous = last;
	last->next->index = last->index + 1;
}

extern struct chalkboard *
chalkboard_new(xcb_connection_t *conn, xcb_window_t win, enum chalkboard_direction dir)
{
	struct chalkboard *c;
	xcb_screen_t *scr;
	int width, height;

	assert(conn != NULL);
	assert(dir == DIRECTION_HORIZONTAL
			|| dir == DIRECTION_VERTICAL);

	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	assert(scr != NULL);

	width = scr->width_in_pixels;
	height = scr->height_in_pixels;

	c = calloc(1, sizeof(struct chalkboard));

	c->dir = dir;
	c->root = __chunk_new(conn, win, width, height);
	c->root->index = 0;

	__chunk_prepend(conn, win, c->root, width, height);
	__chunk_apend(conn, win, c->root, width, height);

	return c;
}

extern void
chalkboard_move(struct chalkboard *c, int offx, int offy)
{
	c->pos.x += offx;
	c->pos.y += offy;
}

extern void
chalkboard_set_viewport(struct chalkboard *c, int vw, int vh)
{
	c->viewport_width = vw;
	c->viewport_height = vh;

	/* TODO: update position to re-center */
	/*       ...                          */
}

extern void
chalkboard_get_full_area(const struct chalkboard *c, int *x, int *y, int *w, int *h)
{
	/* struct chalkboard *first, *last; */
	/* first = __chunk_first(c->root); */
	/* last = __chunk_last(c->root); */
    /*  */
	/* *x = *y = *w = *h = 0; */
    /*  */
	/* if (c->dir == DIRECTION_HORIZONTAL) { */
	/* 	*w = (last->index - first->index + 1) * c->root->width; */
	/* 	*h = c->root->height; */
	/* } else { */
	/* 	*w = c->root->width; */
	/* 	*h = (last->index - first->index + 1) * c->root->height; */
	/* } */
    /*  */
	/* *x = c->pos.x; */
	/* *y = c->pos.x; */
}

extern void
chalkboard_render(xcb_connection_t *conn, xcb_window_t win, struct chalkboard *c)
{
	int cminx, cminy, cmaxx, cmaxy;
	int minx, miny, maxx, maxy;
	const struct chunk *first, *chunk;

	/* xcb_clear_area(conn, 0, win, 0, 0, c->root->width, c->root->height); */

	minx = c->pos.x;
	miny = c->pos.y;
	maxx = minx + c->viewport_width;
	maxy = miny + c->viewport_height;

	first = __chunk_first(c->root);

	for (chunk = first; chunk; chunk = chunk->next) {
		if (c->dir == DIRECTION_VERTICAL) {
			cminx = 0;
			cminy = chunk->index * chunk->height;
		} else {
			cminx = chunk->index * chunk->width;
			cminy = 0;
		}

		cmaxx = cminx + chunk->width;
		cmaxy = cminy + chunk->height;

		if (minx < cmaxx && miny < cmaxy
				&& maxx > cminx && maxy > cminy) {
			xcb_copy_area(conn, chunk->x.shm.pixmap, win,
					chunk->gc, 0, 0, cminx - c->pos.x, cminy - c->pos.y,
					chunk->width, chunk->height);
		}
	}

	xcb_flush(conn);
}

extern void
chalkboard_set_pixel(struct chalkboard *c, int x, int y, uint32_t color)
{
	int cminx, cminy, cmaxx, cmaxy;
	struct chunk *chunk;

	x += c->pos.x;
	y += c->pos.y;

	for (chunk = __chunk_first(c->root); chunk; chunk = chunk->next) {
		if (c->dir == DIRECTION_VERTICAL) {
			cminx = 0;
			cminy = chunk->index * chunk->height;
		} else {
			cminx = chunk->index * chunk->width;
			cminy = 0;
		}

		cmaxx = cminx + chunk->width;
		cmaxy = cminy + chunk->height;

		if (x >= cminx && x < cmaxx && y >= cminy && y < cmaxy) {
			chunk->px[(y-cminy)*chunk->width+(x-cminx)] = color;
			break;
		}
	}
}

extern int
chalkboard_get_pixel(struct chalkboard *c, int x, int y, uint32_t *color)
{
	int cminx, cminy, cmaxx, cmaxy;
	struct chunk *chunk;

	x += c->pos.x;
	y += c->pos.y;

	for (chunk = __chunk_first(c->root); chunk; chunk = chunk->next) {
		if (c->dir == DIRECTION_VERTICAL) {
			cminx = 0;
			cminy = chunk->index * chunk->height;
		} else {
			cminx = chunk->index * chunk->width;
			cminy = 0;
		}

		cmaxx = cminx + chunk->width;
		cmaxy = cminy + chunk->height;

		if (x >= cminx && x < cmaxx && y >= cminy && y < cmaxy) {
			*color = chunk->px[(y-cminy)*chunk->width+(x-cminx)];
			return 1;
		}
	}

	return 0;
}
