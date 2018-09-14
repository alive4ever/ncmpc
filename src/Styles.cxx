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

#include "Styles.hxx"
#include "BasicColors.hxx"
#include "CustomColors.hxx"
#include "i18n.h"

#ifdef ENABLE_COLORS
#include "options.hxx"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

/**
 * Use the terminal's default color.
 *
 * @see init_pair(3ncurses)
 */
static constexpr short COLOR_NONE = -1;

/**
 * A non-standad magic value which means "inherit this color from the
 * parent style".
 */
static constexpr short COLOR_INHERIT = -2;

/**
 * A non-standad magic value which means "inherit attributes from the
 * parent style".
 */
static constexpr attr_t A_INHERIT = ~attr_t(0);

struct StyleData {
	/**
	 * A name which can be used to address this style from the
	 * configuration file.
	 */
	const char *const name;

#ifdef ENABLE_COLORS
	/**
	 * Inherit unspecified values from this style.  The special
	 * value #Style::DEFAULT means "don't inherit".
	 */
	const Style inherit;

	/**
	 * The foreground (text) color in "color" mode.
	 */
	short fg_color;

	/**
	 * The attributes in "color" mode.
	 */
	attr_t attr;
#endif

	/**
	 * The attributes in "mono" mode.
	 */
	const attr_t mono;

#ifndef ENABLE_COLORS
	constexpr StyleData(const char *_name, Style,
			    short, attr_t, attr_t _mono)
		:name(_name), mono(_mono) {}
#endif
};

#ifndef ENABLE_COLORS
constexpr
#endif
static StyleData styles[size_t(Style::END)] = {
	/* color pair = field name, color, mono */
	{
		nullptr, Style::DEFAULT,
		COLOR_NONE, A_NORMAL,
		A_NORMAL,
	},
	{
		"title", Style::DEFAULT,
		COLOR_YELLOW, A_NORMAL,
		A_NORMAL,
	},
	{
		"title-bold", Style::TITLE,
		COLOR_INHERIT, A_BOLD,
		A_BOLD,
	},
	{
		"line", Style::DEFAULT,
		COLOR_WHITE, A_NORMAL,
		A_NORMAL,
	},
	{
		"line-bold", Style::LINE,
		COLOR_INHERIT, A_BOLD,
		A_BOLD,
	},
	{
		"line-flags", Style::LINE,
		COLOR_YELLOW, A_NORMAL,
		A_NORMAL,
	},
	{
		"list", Style::DEFAULT,
		COLOR_GREEN, A_NORMAL,
		A_NORMAL,
	},
	{
		"list-bold", Style::LIST,
		COLOR_INHERIT, A_BOLD,
		A_BOLD,
	},
	{
		"progressbar", Style::DEFAULT,
		COLOR_WHITE, A_NORMAL,
		A_NORMAL,
	},
	{
		"progressbar-background", Style::PROGRESSBAR,
		COLOR_BLACK, A_BOLD,
		A_NORMAL,
	},
	{
		"status-song", Style::DEFAULT,
		COLOR_YELLOW, A_NORMAL,
		A_NORMAL,
	},
	{
		"status-state", Style::STATUS,
		COLOR_INHERIT, A_BOLD,
		A_BOLD,
	},
	{
		"status-time", Style::STATUS,
		COLOR_RED, A_INHERIT,
		A_NORMAL,
	},
	{
		"alert", Style::STATUS,
		COLOR_RED, A_BOLD,
		A_BOLD,
	},
	{
		"browser-directory", Style::LIST,
		COLOR_YELLOW, A_INHERIT,
		A_NORMAL,
	},
	{
		"browser-playlist", Style::LIST,
		COLOR_RED, A_INHERIT,
		A_NORMAL,
	},
	{
		"background", Style::DEFAULT,
		COLOR_BLACK, A_NORMAL,
		A_NORMAL,
	},
};

static constexpr auto &
GetStyle(Style style)
{
	return styles[size_t(style)];
}

#ifdef ENABLE_COLORS

static StyleData *
StyleByName(const char *name)
{
	for (size_t i = 1; i < size_t(Style::END); ++i)
		if (!strcasecmp(styles[i].name, name))
			return &styles[i];

	return nullptr;
}

static void
colors_update_pair(Style style)
{
	auto &data = GetStyle(style);

	int fg = data.fg_color;
	for (Style i = style; fg == COLOR_INHERIT;) {
		i = GetStyle(i).inherit;
		assert(i != Style::DEFAULT);
		fg = GetStyle(i).fg_color;
	}

	int bg = GetStyle(Style::BACKGROUND).fg_color;

	/* apply A_INHERIT (modifies the "attr" value, which is
	   irreversible) */
	for (Style i = style; data.attr == A_INHERIT;) {
		i = GetStyle(i).inherit;
		assert(i != Style::DEFAULT);
		data.attr = GetStyle(i).attr;
	}

	init_pair(short(style), fg, bg);
}

static bool
colors_str2color(const char *str, short &fg_color, attr_t &attr)
{
	char **parts = g_strsplit(str, ",", 0);
	for (int i = 0; parts[i]; i++) {
		char *cur = parts[i];

		/* Legacy colors (brightblue,etc) */
		if (!strncasecmp(cur, "bright", 6)) {
			attr |= A_BOLD;
			cur += 6;
		}

		/* Colors */
		short b = ParseBasicColorName(cur);
		if (b >= 0) {
			fg_color = b;
			continue;
		}

		if (!strcasecmp(cur, "none"))
			fg_color = COLOR_NONE;
		else if (!strcasecmp(cur, "grey") ||
			 !strcasecmp(cur, "gray")) {
			fg_color = COLOR_BLACK;
			attr |= A_BOLD;
		}

		/* Attributes */
		else if (!strcasecmp(cur, "standout"))
			attr |= A_STANDOUT;
		else if (!strcasecmp(cur, "underline"))
			attr |= A_UNDERLINE;
		else if (!strcasecmp(cur, "reverse"))
			attr |= A_REVERSE;
		else if (!strcasecmp(cur, "blink"))
			attr |= A_BLINK;
		else if (!strcasecmp(cur, "dim"))
			attr |= A_DIM;
		else if (!strcasecmp(cur, "bold"))
			attr |= A_BOLD;
		else {
			/* Numerical colors */
			char *endptr;
			int tmp = strtol(cur, &endptr, 10);
			if (cur != endptr && endptr[0] == '\0') {
				fg_color = tmp;
			} else {
				fprintf(stderr, "%s: %s\n",
					_("Unknown color"), str);
				return false;
			}
		}

	}
	g_strfreev(parts);
	return true;
}

bool
ModifyStyle(const char *name, const char *value)
{
	auto *entry = StyleByName(name);

	if (!entry) {
		fprintf(stderr, "%s: %s",
			_("Unknown color field"), name);
		return false;
	}

	return colors_str2color(value, entry->fg_color, entry->attr);
}

void
ApplyStyles()
{
	if (has_colors()) {
		/* initialize color support */
		start_color();
		use_default_colors();
		/* define any custom colors defined in the configuration file */
		ApplyCustomColors();

		if (options.enable_colors) {
			for (size_t i = 1; i < size_t(Style::END); ++i)
				/* update the color pairs */
				colors_update_pair(Style(i));
		}
	} else if (options.enable_colors) {
		fprintf(stderr, "%s\n",
			_("Terminal lacks color capabilities"));
		options.enable_colors = false;
	}
}
#endif

void
SelectStyle(WINDOW *w, Style style)
{
	const auto &data = GetStyle(style);

#ifdef ENABLE_COLORS
	if (options.enable_colors) {
		/* color mode */
		wattr_set(w, data.attr, short(style), nullptr);
	} else {
#endif
		/* mono mode */
		(void)wattrset(w, data.mono);
#ifdef ENABLE_COLORS
	}
#endif
}