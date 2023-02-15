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

#include <stdint.h>

#include "pizarra.h"

typedef enum {
	HISTORY_ADD_POINT
		// TODO(alpheratz): Maybe history_drag_action?
} HistoryAtomicActionKind;

typedef struct {
	HistoryAtomicActionKind kind;
	union {
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
history_atomic_action_add_point_new(int x, int y, uint32_t color, int size);

extern void
history_user_action_push_atomic(HistoryUserAction *hua, HistoryAtomicAction *hac);

extern void
history_do(History *hist, HistoryUserAction *hua);

extern void
history_undo(History *hist);

extern void
history_print(const History *hist);

extern void
history_destroy(History *hist);
