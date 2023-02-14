#pragma once

#include <stdint.h>

#include "pizarra.h"

typedef enum {
	HISTORY_DRAW_POINT,
	HISTORY_CHANGE_COLOR,
	HISTORY_ADD_POINT
		// TODO(alpheratz): Maybe history_drag_action?
} HistoryAtomicActionKind;

typedef struct {
	HistoryAtomicActionKind kind;
	union {
		struct {
			int x;
			int y;
		} dp; /* HISTORY_DRAW_POINT */
		struct {
			uint32_t color;
		} cc; /* HISTORY_CHANGE_COLOR */
		struct {
			int x;
			int y;
			uint32_t color;
			int size;
		} ap; /* HISTORY_ADD_POINT */
	} info;
} HistoryAtomicAction;

typedef struct {
	size_t n;
	HistoryAtomicAction **aa;
} HistoryUserAction;

typedef struct {
	size_t len;
	size_t capacity;
	HistoryUserAction **actions;
} History;

extern History *
history_new(void);

extern HistoryUserAction *
history_user_action_new(void);

extern HistoryAtomicAction *
history_atomic_action_draw_point_new(int x, int y);

extern HistoryAtomicAction *
history_atomic_action_change_color_new(uint32_t color);

extern HistoryAtomicAction *
history_atomic_action_add_point_new(int x, int y, uint32_t color, int size);

extern void
history_user_action_push_atomic(HistoryUserAction *hua, HistoryAtomicAction *hac);

extern void
history_do(History *hist, HistoryUserAction *hua);

extern void
history_undo(History *hist);

extern void
history_print(const History *hist);
