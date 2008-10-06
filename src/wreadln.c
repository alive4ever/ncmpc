/*
 * (c) 2004 by Kalle Wallin <kaw@linux.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "wreadln.h"
#include "charset.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#define KEY_CTRL_A   1
#define KEY_CTRL_B   2
#define KEY_CTRL_C   3
#define KEY_CTRL_D   4
#define KEY_CTRL_E   5
#define KEY_CTRL_F   6
#define KEY_CTRL_G   7
#define KEY_CTRL_K   11
#define KEY_CTRL_N   14
#define KEY_CTRL_P   16
#define KEY_CTRL_U   21
#define KEY_CTRL_Z   26
#define KEY_BCKSPC   8
#define TAB          9

#define WRLN_MAX_LINE_SIZE 1024
#define WRLN_MAX_HISTORY_LENGTH 32
 
guint wrln_max_line_size = WRLN_MAX_LINE_SIZE;
guint wrln_max_history_length = WRLN_MAX_HISTORY_LENGTH;
void *wrln_completion_callback_data = NULL;
wrln_gcmp_pre_cb_t wrln_pre_completion_callback = NULL;
wrln_gcmp_post_cb_t wrln_post_completion_callback = NULL;

extern void sigstop(void);
extern void screen_bell(void);

/* move the cursor one step to the right */
static inline void cursor_move_right(gint *cursor,
				     gint *start,
				     gint width,
				     gint x0,
				     gint x1,
				     gchar *line)
{
	if (*cursor < (int)strlen(line) &&
	    *cursor < (int)wrln_max_line_size - 1) {
		(*cursor)++;
		if (*cursor + x0 >= x1 && *start < *cursor - width + 1)
			(*start)++;
	}
}

/* move the cursor one step to the left */
static inline void cursor_move_left(gint *cursor,
                                    gint *start)
{
	if (*cursor > 0) {
		if (*cursor == *start && *start > 0)
			(*start)--;
		(*cursor)--;
	}
}

/* move the cursor to the end of the line */
static inline void cursor_move_to_eol(gint *cursor,
				      gint *start,
				      gint width,
				      gint x0,
				      gint x1,
				      gchar *line)
{
	*cursor = strlen(line);
	if (*cursor + x0 >= x1)
		*start = *cursor - width + 1;
}

/* draw line buffer and update cursor position */
static inline void drawline(gint cursor,
			    gint start,
			    gint width,
			    gint x0,
			    gint y,
			    gboolean masked,
			    gchar *line,
			    WINDOW *w)
{
	wmove(w, y, x0);
	/* clear input area */
	whline(w, ' ', width);
	/* print visible part of the line buffer */
	if(masked == TRUE)
		whline(w, '*', utf8_width(line) - start);
	else
		waddnstr(w, line+start, width);
	/* move the cursor to the correct position */
	wmove(w, y, x0 + cursor-start);
	/* tell ncurses to redraw the screen */
	doupdate();
}

/* libcurses version */

static gchar *
_wreadln(WINDOW *w,
	 const gchar *prompt,
	 const gchar *initial_value,
	 gint x1,
	 GList **history,
	 GCompletion *gcmp,
	 gboolean masked)
{
	GList *hlist = NULL, *hcurrent = NULL;
	gchar *line;
	gint x0, y, width;
	gint cursor = 0, start = 0;
	gint key = 0, i;

	/* allocate a line buffer */
	line = g_malloc0(wrln_max_line_size);
	/* turn off echo */
	noecho();
	/* make shure the cursor is visible */
	curs_set(1);
	/* print prompt string */
	if (prompt)
		waddstr(w, prompt);
	/* retrive y and x0 position */
	getyx(w, y, x0);
	/* check the x1 value */
	if (x1 <= x0 || x1 > COLS)
		x1 = COLS;
	width = x1 - x0;
	/* clear input area */
	mvwhline(w, y, x0, ' ', width);

	if (history) {
		/* append the a new line to our history list */
		*history = g_list_append(*history, g_malloc0(wrln_max_line_size));
		/* hlist points to the current item in the history list */
		hlist = g_list_last(*history);
		hcurrent = hlist;
	}

	if (initial_value == (char *)-1) {
		/* get previous history entry */
		if (history && hlist->prev) {
			if (hlist == hcurrent)
				/* save the current line */
				g_strlcpy(hlist->data, line, wrln_max_line_size);

			/* get previous line */
			hlist = hlist->prev;
			g_strlcpy(line, hlist->data, wrln_max_line_size);
		}
		cursor_move_to_eol(&cursor, &start, width, x0, x1, line);
		drawline(cursor, start, width, x0, y, masked, line, w);
	} else if (initial_value) {
		/* copy the initial value to the line buffer */
		g_strlcpy(line, initial_value, wrln_max_line_size);
		cursor_move_to_eol(&cursor, &start, width, x0, x1, line);
		drawline(cursor, start, width, x0, y, masked, line, w);
	}

	while (key != 13 && key != '\n') {
		key = wgetch(w);

		/* check if key is a function key */
		for (i = 0; i < 63; i++)
			if (key == KEY_F(i)) {
				key = KEY_F(1);
				i = 64;
			}

		switch (key) {
#ifdef HAVE_GETMOUSE
		case KEY_MOUSE: /* ignore mouse events */
#endif
		case ERR: /* ingnore errors */
			break;

		case TAB:
			if (gcmp) {
				char *prefix = NULL;
				GList *list;

				if (wrln_pre_completion_callback)
					wrln_pre_completion_callback(gcmp, line,
								     wrln_completion_callback_data);
				list = g_completion_complete(gcmp, line, &prefix);
				if (prefix) {
					g_strlcpy(line, prefix, wrln_max_line_size);
					cursor_move_to_eol(&cursor, &start, width, x0, x1, line);
					g_free(prefix);
				} else
					screen_bell();

				if (wrln_post_completion_callback)
					wrln_post_completion_callback(gcmp, line, list,
								      wrln_completion_callback_data);
			}
			break;

		case KEY_CTRL_G:
			screen_bell();
			g_free(line);
			if (history) {
				g_free(hcurrent->data);
				hcurrent->data = NULL;
				*history = g_list_delete_link(*history, hcurrent);
			}
			return NULL;

		case KEY_LEFT:
		case KEY_CTRL_B:
			cursor_move_left(&cursor, &start);
			break;
		case KEY_RIGHT:
		case KEY_CTRL_F:
			cursor_move_right(&cursor, &start, width, x0, x1, line);
			break;
		case KEY_HOME:
		case KEY_CTRL_A:
			cursor = 0;
			start = 0;
			break;
		case KEY_END:
		case KEY_CTRL_E:
			cursor_move_to_eol(&cursor, &start, width, x0, x1, line);
			break;
		case KEY_CTRL_K:
			line[cursor] = 0;
			break;
		case KEY_CTRL_U:
			cursor = utf8_width(line);
			for (i = 0;i < cursor; i++)
				line[i] = '\0';
			cursor = 0;
			break;
		case 127:
		case KEY_BCKSPC:	/* handle backspace: copy all */
		case KEY_BACKSPACE:	/* chars starting from curpos */
			if( cursor > 0 ) {/* - 1 from buf[n+1] to buf   */
				for (i = cursor - 1; line[i] != 0; i++)
					line[i] = line[i + 1];
				cursor_move_left(&cursor, &start);
			}
			break;
		case KEY_DC:		/* handle delete key. As above */
		case KEY_CTRL_D:
			if (cursor <= (gint)utf8_width(line) - 1) {
				for (i = cursor; line[i] != 0; i++)
					line[i] = line[i + 1];
			}
			break;
		case KEY_UP:
		case KEY_CTRL_P:
			/* get previous history entry */
			if (history && hlist->prev) {
				if (hlist == hcurrent)
					/* save the current line */
					g_strlcpy(hlist->data, line, wrln_max_line_size);

				/* get previous line */
				hlist = hlist->prev;
				g_strlcpy(line, hlist->data, wrln_max_line_size);
			}
			cursor_move_to_eol(&cursor, &start, width, x0, x1, line);
			break;
		case KEY_DOWN:
		case KEY_CTRL_N:
			/* get next history entry */
			if (history && hlist->next) {
				/* get next line */
				hlist = hlist->next;
				g_strlcpy(line, hlist->data, wrln_max_line_size);
			}
			cursor_move_to_eol(&cursor, &start, width, x0, x1, line);
			break;

		case '\n':
		case 13:
		case KEY_IC:
		case KEY_PPAGE:
		case KEY_NPAGE:
		case KEY_F(1):
			/* ignore char */
			break;
		default:
			if (key >= 32) {
				if (strlen (line + cursor)) { /* if the cursor is */
					/* not at the last pos */
					gchar *tmp = NULL;
					gsize size = strlen(line + cursor) + 1;

					tmp = g_malloc0(size);
					g_strlcpy (tmp, line + cursor, size);
					line[cursor] = key;
					line[cursor + 1] = 0;
					g_strlcat (&line[cursor + 1], tmp, size);
					g_free(tmp);
					cursor_move_right(&cursor, &start, width, x0, x1, line);
				} else {
					line[cursor + 1] = 0;
					line[cursor] = key;
					cursor_move_right(&cursor, &start, width, x0, x1, line);
				}
			}
		}

		drawline(cursor, start, width, x0, y, masked, line, w);
	}

	/* update history */
	if (history) {
		if (strlen(line)) {
			/* update the current history entry */
			size_t size = strlen(line)+1;
			hcurrent->data = g_realloc(hcurrent->data, size);
			g_strlcpy(hcurrent->data, line, size);
		} else {
			/* the line was empty - remove the current history entry */
			g_free(hcurrent->data);
			hcurrent->data = NULL;
			*history = g_list_delete_link(*history, hcurrent);
		}

		while (g_list_length(*history) > wrln_max_history_length) {
			GList *first = g_list_first(*history);

			/* remove the oldest history entry  */
			g_free(first->data);
			first->data = NULL;
			*history = g_list_delete_link(*history, first);
		}
	}

	return g_realloc(line, strlen(line)+1);
}

gchar *
wreadln(WINDOW *w,
	const gchar *prompt,
	const gchar *initial_value,
	gint x1,
	GList **history,
	GCompletion *gcmp)
{
	return  _wreadln(w, prompt, initial_value, x1, history, gcmp, FALSE);
}

gchar *
wreadln_masked(WINDOW *w,
	       const gchar *prompt,
	       const gchar *initial_value,
	       gint x1,
	       GList **history,
	       GCompletion *gcmp)
{
	return  _wreadln(w, prompt, initial_value, x1, history, gcmp, TRUE);
}
