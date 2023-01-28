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
	struct vec2 pos;
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
	struct vec2 off; /* offset from origin */
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
__chunk_new(xcb_connection_t *conn, xcb_window_t win, int x, int y, int w, int h)
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

	c->pos.x = x;
	c->pos.y = y;
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
		memset(c->px, 255, szpx);

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
	c->root = __chunk_new(conn, win, 0, 0, width, height);

	if (dir == DIRECTION_HORIZONTAL) {
		c->root->previous = __chunk_new(conn, win, -width, 0, width, height);
		c->root->next = __chunk_new(conn, win, width, 0, width, height);
	} else {
		c->root->previous = __chunk_new(conn, win, 0, -height, width, height);
		c->root->next = __chunk_new(conn, win, 0, height, width, height);
	}

	c->root->previous->next = c->root;
	c->root->next->previous = c->root;

	return c;
}

extern void
chalkboard_render(xcb_connection_t *conn, xcb_window_t win, struct chalkboard *c)
{
	xcb_copy_area(conn, c->root->x.shm.pixmap, win,
			c->root->gc, 0, 0, 0, 0,
			c->root->width, c->root->height);

	xcb_flush(conn);
}

/* static int */
/* __chunk_intersect(const struct chunk *c, int x, int y, int w, int h) */
/* { */
/* 	return x < (c->pos.x + c->width)     && */
/* 		   y < (c->pos.y + c->height)    && */
/* 		   (x + w) > c->pos.x            && */
/* 		   (y + h) > c->pos.y; */
/* } */
