/****************************************************************************
 * Copyright (c) 2017 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey                                                *
 ****************************************************************************/

/* new_pair.c
 *
 * New color-pair functions, alloc_pair and free_pair
 */

#define NEW_PAIR_INTERNAL 1
#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

#ifdef USE_TERM_DRIVER
#define MaxColors      InfoOf(SP_PARM).maxcolors
#else
#define MaxColors      max_colors
#endif

#if USE_NEW_PAIR

/* fix redefinition versys tic.h */
#undef entry
#define entry my_entry
#undef ENTRY
#define ENTRY my_ENTRY

#include <search.h>

#endif

MODULE_ID("$Id: new_pair.c,v 1.8 2017/03/10 09:22:50 tom Exp $")

#if USE_NEW_PAIR

#ifdef NEW_PAIR_DEBUG

static int
prev_len(SCREEN *sp, int pair)
{
    int result = 1;
    int base = pair;
    colorpair_t *list = sp->_color_pairs;
    while (list[pair].prev != base) {
	result++;
	pair = list[pair].prev;
    }
    return result;
}

static int
next_len(SCREEN *sp, int pair)
{
    int result = 1;
    int base = pair;
    colorpair_t *list = sp->_color_pairs;
    while (list[pair].next != base) {
	result++;
	pair = list[pair].next;
    }
    return result;
}

/*
 * Trace the contents of LRU color-pairs.
 */
static void
dumpit(SCREEN *sp, int pair, const char *tag)
{
    colorpair_t *list = sp->_color_pairs;
    char bigbuf[256 * 20];
    char *p = bigbuf;
    int n;
    sprintf(p, "%s", tag);
    p += strlen(p);
    for (n = 0; n < sp->_pair_limit; ++n) {
	if (list[n].mode != cpFREE) {
	    sprintf(p, " %d%c(%d,%d)",
		    n, n == pair ? '@' : ':', list[n].next, list[n].prev);
	    p += strlen(p);
	}
    }
    T(("(%d/%d) %ld - %s",
       next_len(sp, 0),
       prev_len(sp, 0),
       strlen(bigbuf), bigbuf));

    if (next_len(sp, 0) != prev_len(sp, 0)) {
	endwin();
	exit(1);
    }
}
#else
#define dumpit(sp, pair, tag)	/* nothing */
#endif

static int
compare_data(const void *a, const void *b)
{
    const colorpair_t *p = (const colorpair_t *) a;
    const colorpair_t *q = (const colorpair_t *) b;
    return ((p->fg == q->fg)
	    ? (p->bg - q->bg)
	    : (p->fg - q->fg));
}

static int
_nc_find_color_pair(SCREEN *sp, int fg, int bg)
{
    colorpair_t find;
    int result;
    void *pp;

    find.fg = fg;
    find.bg = bg;
    if ((pp = tfind(&find, &sp->_ordered_pairs, compare_data)) != 0) {
	colorpair_t *temp = *(colorpair_t **) pp;
	result = (int) (temp - sp->_color_pairs);
    } else {
	result = -1;
    }
    return result;
}

static void
delink_color_pair(SCREEN *sp, int pair)
{
    colorpair_t *list = sp->_color_pairs;
    int prev = list[pair].prev;
    int next = list[pair].next;

    /* delink this from its current location */
    if (list[prev].next == pair &&
	list[next].prev == pair) {
	list[prev].next = next;
	list[next].prev = prev;
	dumpit(sp, pair, "delinked");
    }
}

/*
 * Use this call to update the fast-index when modifying an entry in the color
 * pair table.
 */
NCURSES_EXPORT(void)
_nc_reset_color_pair(SCREEN *sp, int pair, colorpair_t * next)
{
    if (ValidPair(sp, pair)) {
	colorpair_t *last = &(sp->_color_pairs[pair]);
	delink_color_pair(sp, pair);
	if (last->mode > cpFREE &&
	    (last->fg != next->fg || last->bg != next->bg)) {
	    /* remove the old entry from fast index */
	    tdelete(last, &sp->_ordered_pairs, compare_data);
	    /* create a new entry in fast index */
	    *last = *next;
	    tsearch(last, &sp->_ordered_pairs, compare_data);
	}
    }
}

/*
 * Use this call to relink the newest pair to the front of the list, keeping
 * "0" first.
 */
NCURSES_EXPORT(void)
_nc_set_color_pair(SCREEN *sp, int pair, int mode)
{
    if (ValidPair(sp, pair)) {
	colorpair_t *list = sp->_color_pairs;
	dumpit(sp, pair, "SET_PAIR");
	list[0].mode = cpKEEP;
	if (list[pair].mode <= cpFREE)
	    sp->_pairs_used++;
	list[pair].mode = mode;
	if (list[0].next != pair) {
	    /* link it at the front of the list */
	    list[pair].next = list[0].next;
	    list[list[pair].next].prev = pair;
	    list[pair].prev = 0;
	    list[0].next = pair;
	}
	dumpit(sp, pair, "...after");
    }
}

NCURSES_EXPORT(int)
NCURSES_SP_NAME(alloc_pair) (NCURSES_SP_DCLx int fg, int bg)
{
    int pair;

    T((T_CALLED("alloc_pair(%d,%d)"), fg, bg));
    if ((pair = _nc_find_color_pair(SP_PARM, fg, bg)) < 0) {
	/*
	 * Check if all of the slots have been used.  If not, find one and
	 * use that.
	 */
	if (SP_PARM->_pairs_used + 1 < SP_PARM->_pair_limit) {
	    bool found = FALSE;
	    int hint = SP_PARM->_recent_pair;

	    /*
	     * The linear search is done to allow mixing calls to init_pair()
	     * and alloc_pair().  The former can make gaps...
	     */
	    for (pair = hint + 1; pair < SP_PARM->_pair_limit; pair++) {
		if (SP_PARM->_color_pairs[pair].mode == cpFREE) {
		    T(("found gap %d", pair));
		    found = TRUE;
		    break;
		}
	    }
	    if (!found) {
		for (pair = 1; pair <= hint; pair++) {
		    if (SP_PARM->_color_pairs[pair].mode == cpFREE) {
			T(("found gap %d", pair));
			found = TRUE;
			break;
		    }
		}
	    }
	    if (found) {
		SP_PARM->_recent_pair = pair;
	    } else {
		pair = ERR;
	    }
	} else {
	    /* reuse the oldest one */
	    pair = SP_PARM->_color_pairs[0].prev;
	    T(("reusing %d", pair));
	}

	if (pair > 0 && pair <= MAX_XCURSES_PAIR) {
	    IGNORE_RC(init_pair((short)pair, (short)fg, (short)bg));
	} else {
	    pair = ERR;
	}
    }
    returnCode(pair);
}

NCURSES_EXPORT(int)
NCURSES_SP_NAME(find_pair) (NCURSES_SP_DCLx int fg, int bg)
{
    int pair;

    T((T_CALLED("find_pair(%d,%d)"), fg, bg));
    pair = _nc_find_color_pair(SP_PARM, fg, bg);
    returnCode(pair);
}

NCURSES_EXPORT(int)
NCURSES_SP_NAME(free_pair) (NCURSES_SP_DCLx int pair)
{
    int result = ERR;
    T((T_CALLED("free_pair(%d)"), pair));
    if (ValidPair(SP_PARM, pair)) {
	colorpair_t *cp = &(SP_PARM->_color_pairs[pair]);
	if (pair != 0) {
	    _nc_change_pair(SP_PARM, pair);
	    delink_color_pair(SP_PARM, pair);
	    tdelete(cp, &SP_PARM->_ordered_pairs, compare_data);
	    cp->mode = cpFREE;
	    result = OK;
	    SP_PARM->_pairs_used--;
	}
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
alloc_pair(int f, int b)
{
    return NCURSES_SP_NAME(alloc_pair) (CURRENT_SCREEN, f, b);
}

NCURSES_EXPORT(int)
find_pair(int f, int b)
{
    return NCURSES_SP_NAME(find_pair) (CURRENT_SCREEN, f, b);
}

NCURSES_EXPORT(int)
free_pair(int pair)
{
    return NCURSES_SP_NAME(free_pair) (CURRENT_SCREEN, pair);
}
#endif

#if NO_LEAKS
NCURSES_EXPORT(void)
_nc_new_pair_leaks(SCREEN *sp)
{
    if (sp->_color_pairs) {
	while (sp->_color_pairs[0].next) {
	    free_pair(sp->_color_pairs[0].next);
	}
    }
}
#endif

#else
void _nc_new_pair(void);
void
_nc_new_pair(void)
{
};
#endif /* USE_NEW_PAIR */
