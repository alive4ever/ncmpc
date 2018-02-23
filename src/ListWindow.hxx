/* ncmpc (Ncurses MPD Client)
 * (c) 2004-2018 The Music Player Daemon Project
 * Project homepage: http://musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIST_WINDOW_HXX
#define LIST_WINDOW_HXX

#include "config.h"
#include "command.hxx"
#include "colors.hxx"
#include "ncmpc_curses.h"

typedef const char *
(*list_window_callback_fn_t)(unsigned i, void *data);

typedef void
(*list_window_paint_callback_t)(WINDOW *w, unsigned i,
				unsigned y, unsigned width,
				bool selected,
				const void *data);

/**
 * The bounds of a range selection, see list_window_get_range().
 */
struct ListWindowRange {
	/**
	 * The index of the first selected item.
	 */
	unsigned start;

	/**
	 * The index after the last selected item.  The selection is
	 * empty when this is the same as "start".
	 */
	unsigned end;

	constexpr bool Contains(unsigned i) const {
		return i >= start && i < end;
	}
};

struct ListWindow {
	WINDOW *w;
	unsigned rows, cols;

	/**
	 * Number of items in this list.
	 */
	unsigned length = 0;

	unsigned start = 0;
	unsigned selected = 0;

	/**
	 * Represents the base item.
	 */
	unsigned range_base = 0;

	/**
	 * Range selection activated?
	 */
	bool range_selection = false;

	bool hide_cursor = false;

	ListWindow(WINDOW *_w, unsigned _cols, unsigned _rows)
		:w(_w), rows(_rows), cols(_cols) {}

	/** reset a list window (selected=0, start=0) */
	void Reset();

	void Resize(unsigned width, unsigned height);

	void SetLength(unsigned length);

	void Paint(list_window_callback_fn_t callback,
		   void *callback_data) const;

	void Paint(list_window_paint_callback_t paint_callback,
		   const void *callback_data) const;

	/** perform basic list window commands (movement) */
	bool HandleCommand(command_t cmd);

	/**
	 * Scroll the window.  Returns true if the command has been
	 * consumed.
	 */
	bool HandleScrollCommand(command_t cmd);

#ifdef HAVE_GETMOUSE
	/**
	 * The mouse was clicked.  Check if the list should be scrolled
	 * Returns non-zero if the mouse event has been handled.
	 */
	bool HandleMouse(mmask_t bstate, int y);
#endif

	/**
	 * Centers the visible range around item n on the list.
	 */
	void Center(unsigned n);

	/**
	 * Scrolls the view to item n, as if the cursor would have been moved
	 * to the position.
	 */
	void ScrollTo(unsigned n);

	/**
	 * Sets the position of the cursor.  Disables range selection.
	 */
	void SetCursor(unsigned i);

	/**
	 * Moves the cursor.  Modifies the range if range selection is
	 * enabled.
	 */
	void MoveCursor(unsigned n);

	void MoveCursorNext();
	void MoveCursorPrevious();
	void MoveCursorTop();
	void MoveCursorMiddle();
	void MoveCursorBottom();
	void MoveCursorFirst();
	void MoveCursorLast();
	void MoveCursorNextPage();
	void MoveCursorPreviousPage();

	void ScrollUp(unsigned n);
	void ScrollDown(unsigned n);

	/**
	 * Ensures that the cursor is visible on the screen, i.e. it is not
	 * outside the current scrolling range.
	 */
	void FetchCursor();

	/**
	 * Determines the lower and upper bound of the range selection.  If
	 * range selection is disabled, it returns the cursor position (range
	 * length is 1).
	 */
	gcc_pure
	ListWindowRange GetRange() const;

	/**
	 * Find a string in a list window.
	 */
	bool Find(list_window_callback_fn_t callback, void *callback_data,
		  const char *str,
		  bool wrap,
		  bool bell_on_wrap);

	/**
	 * Find a string in a list window (reversed).
	 */
	bool ReverseFind(list_window_callback_fn_t callback,
			 void *callback_data,
			 const char *str,
			 bool wrap,
			 bool bell_on_wrap);

	/**
	 * Find a string in a list window which begins with the given
	 * characters in *str.
	 */
	bool Jump(list_window_callback_fn_t callback, void *callback_data,
		  const char *str);

private:
	gcc_pure
	unsigned ValidateIndex(unsigned i) const;

	void CheckSelected();

	/**
	 * Scroll after the cursor was moved, the list was changed or
	 * the window was resized.
	 */
	void CheckOrigin() {
		ScrollTo(selected);
	}
};

#endif
