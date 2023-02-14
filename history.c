#include <stdio.h>
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
	while (hua->n-- > 0)
		history_atomic_action_free(hua->aa[hua->n]);
	free(hua->aa);
	free(hua);
}

extern History *
history_new(void)
{
	History *hist;
	hist = malloc(sizeof(History));
	hist->capacity = 0;
	hist->len = 0;
	hist->actions = NULL;
	return hist;
}

extern HistoryUserAction *
history_user_action_new(void)
{
	HistoryUserAction *hua;
	hua = malloc(sizeof(HistoryUserAction));
	hua->n = 0;
	hua->aa = NULL;
	return hua;
}

extern HistoryAtomicAction *
history_atomic_action_draw_point_new(int x, int y)
{
	HistoryAtomicAction *haa;
	haa = malloc(sizeof(HistoryAtomicAction));
	haa->kind = HISTORY_DRAW_POINT;
	haa->info.dp.x = x;
	haa->info.dp.y = y;
	return haa;
}

extern HistoryAtomicAction *
history_atomic_action_change_color_new(uint32_t color)
{
	HistoryAtomicAction *haa;
	haa = malloc(sizeof(HistoryAtomicAction));
	haa->kind = HISTORY_CHANGE_COLOR;
	haa->info.cc.color = color;
	return haa;
}

extern HistoryAtomicAction *
history_atomic_action_add_point_new(int x, int y, uint32_t color, int size)
{
	HistoryAtomicAction *haa;
	haa = malloc(sizeof(HistoryAtomicAction));
	haa->kind = HISTORY_ADD_POINT;
	haa->info.ap.x = x;
	haa->info.ap.y = y;
	haa->info.ap.color = color;
	haa->info.ap.size = size;
	return haa;
}

extern void
history_user_action_push_atomic(HistoryUserAction *hua, HistoryAtomicAction *hac)
{
	hua->n++;

	if (hua->n == 1) {
		hua->aa = malloc(sizeof(HistoryAtomicAction *));
	} else {
		hua->aa = realloc(hua->aa,
				hua->n * sizeof(HistoryAtomicAction *));
	}

	hua->aa[hua->n - 1] = hac;
}

extern void
history_do(History *hist, HistoryUserAction *hua)
{
	hist->len++;
	if (hist->len >= hist->capacity) {
		hist->capacity += 32;
		hist->actions = realloc(hist->actions, hist->capacity * sizeof(HistoryUserAction *));
	}
	hist->actions[hist->len - 1] = hua;
}

extern void
history_undo(History *hist)
{
	if (hist->len == 0)
		return;
	hist->len -= 1;
	history_user_action_free(hist->actions[hist->len]);
	hist->actions[hist->len] = NULL;
}

static void
history_atomic_action_print(const HistoryAtomicAction *haa)
{
	switch (haa->kind) {
	case HISTORY_CHANGE_COLOR:
		printf("\t\tChanging color to #%06x\n", haa->info.cc.color);
		break;
	case HISTORY_DRAW_POINT:
		printf("\t\tDrawing point at x: %d y: %d\n", haa->info.dp.x, haa->info.dp.y);
		break;
	case HISTORY_ADD_POINT:
		printf("\t\tAdding a point at x: %d y: %d with color: %06x and brush size: %d\n",
				haa->info.ap.x, haa->info.ap.y, haa->info.ap.color, haa->info.ap.size);
	}
}

static void
history_user_action_print(const HistoryUserAction *hua)
{
	size_t i;
	HistoryAtomicAction *haa;
	for (i = 0; i < hua->n; ++i) {
		printf("\tAtomic Action #%zu\n", i);
		haa = hua->aa[i];
		history_atomic_action_print(haa);
	}
}

extern void
history_print(const History *hist)
{
	size_t i;
	HistoryUserAction *hua;

	for (i = 0; i < hist->len; ++i) {
		printf("User Action #%zu\n", i);
		hua = hist->actions[i];
		history_user_action_print(hua);
	}
}

/*  */
/* int */
/* main(void) */
/* { */
/* 	History *hist; */
/* 	HistoryUserAction *last_action; */
/*  */
/* 	hist = history_new(); */
/*  */
/* 	last_action = history_user_action_new(); */
/* 	history_user_action_push_atomic(last_action, */
/* 			history_atomic_action_draw_point_new(0, 0)); */
/* 	history_user_action_push_atomic(last_action, */
/* 			history_atomic_action_draw_point_new(0, 1)); */
/* 	history_do(hist, last_action); */
/*  */
/* 	last_action = history_user_action_new(); */
/* 	history_user_action_push_atomic(last_action, */
/* 			history_atomic_action_change_color_new(0xff00f3)); */
/* 	history_do(hist, last_action); */
/*  */
/* 	history_print(hist); */
/*  */
/* 	return 0; */
/* } */
