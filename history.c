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

#include <stdlib.h>

#include "history.h"

static void
history_atomic_action_free(HistoryAtomicAction *haa)
{
	free(haa);
}

static void
history_user_action_free(HistoryUserAction *hua)
{
	HistoryAtomicAction *todelete, *tmp;
	for (todelete = hua->aa; todelete; todelete = tmp) {
		tmp = todelete->next;
		history_atomic_action_free(todelete);
	}
	free(hua);
}

extern History *
history_new(void)
{
	History *hist;
	hist = malloc(sizeof(History));
	hist->root = history_user_action_new();
	hist->current = hist->root;
	return hist;
}

extern HistoryUserAction *
history_user_action_new(void)
{
	HistoryUserAction *hua;
	hua = malloc(sizeof(HistoryUserAction));
	hua->next = NULL;
	hua->prev = NULL;
	hua->aa = NULL;
	return hua;
}

extern HistoryAtomicAction *
history_atomic_action_new(int x, int y, uint32_t color, int size)
{
	HistoryAtomicAction *haa;
	haa = malloc(sizeof(HistoryAtomicAction));
	haa->x = x;
	haa->y = y;
	haa->color = color;
	haa->size = size;
	haa->next = NULL;
	return haa;
}

extern void
history_user_action_push_atomic(HistoryUserAction *hua,
		HistoryAtomicAction *haa)
{
	HistoryAtomicAction *last;

	// check if there is no atomic actions
	// created yet
	if (NULL == hua->aa) {
		hua->aa = haa;
		return;
	}

	// get last node
	for (last = hua->aa; last->next; last = last->next)
		;

	last->next = haa;
}

extern void
history_do(History *hist, HistoryUserAction *hua)
{
	HistoryUserAction *todelete, *tmp;
	for (todelete = hist->current->next; todelete; todelete = tmp) {
		tmp = todelete->next;
		history_user_action_free(todelete);
	}

	// link
	hist->current->next = hua;
	hua->prev = hist->current;

	// update position in history
	hist->current = hua;
}

extern void
history_undo(History *hist)
{
	if (NULL != hist->current->prev)
		hist->current = hist->current->prev;
}

extern void
history_redo(History *hist)
{
	if (NULL != hist->current->next)
		hist->current = hist->current->next;
}

extern void
history_destroy(History *hist)
{
	HistoryUserAction *todelete, *tmp;
	for (todelete = hist->root; todelete; todelete = tmp) {
		tmp = todelete->next;
		history_user_action_free(todelete);
	}
	free(hist);
}
