/*
 * Copyright 2007 Jon Loeliger, Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *                                                                   USA
 */

#define _GNU_SOURCE

#include <stdio.h>

#include "dtc.h"
#include "srcpos.h"


static char *dirname(const char *path)
{
	const char *slash = strrchr(path, '/');

	if (slash) {
		int len = slash - path;
		char *dir = xmalloc(len + 1);

		memcpy(dir, path, len);
		dir[len] = '\0';
		return dir;
	}
	return NULL;
}

struct srcfile_state *current_srcfile; /* = NULL */

/* Detect infinite include recursion. */
#define MAX_SRCFILE_DEPTH     (100)
static int srcfile_depth; /* = 0 */

FILE *srcfile_relative_open(const char *fname, char **fullnamep)
{
	FILE *f;
	char *fullname;

	if (streq(fname, "-")) {
		f = stdin;
		fullname = xstrdup("<stdin>");
	} else {
		if (!current_srcfile || !current_srcfile->dir
		    || (fname[0] == '/'))
			fullname = xstrdup(fname);
		else
			fullname = join_path(current_srcfile->dir, fname);

		f = fopen(fullname, "r");
		if (!f)
			die("Couldn't open \"%s\": %s\n", fname,
			    strerror(errno));
	}

	if (fullnamep)
		*fullnamep = fullname;
	else
		free(fullname);

	return f;
}

void srcfile_push(const char *fname)
{
	struct srcfile_state *srcfile;

	if (srcfile_depth++ >= MAX_SRCFILE_DEPTH)
		die("Includes nested too deeply");

	srcfile = xmalloc(sizeof(*srcfile));

	srcfile->f = srcfile_relative_open(fname, &srcfile->name);
	srcfile->dir = dirname(srcfile->name);
	srcfile->prev = current_srcfile;
	current_srcfile = srcfile;
}

int srcfile_pop(void)
{
	struct srcfile_state *srcfile = current_srcfile;

	assert(srcfile);

	current_srcfile = srcfile->prev;

	if (fclose(srcfile->f))
		die("Error closing \"%s\": %s\n", srcfile->name, strerror(errno));

	/* FIXME: We allow the srcfile_state structure to leak,
	 * because it could still be referenced from a location
	 * variable being carried through the parser somewhere.  To
	 * fix this we could either allocate all the files from a
	 * table, or use a pool allocator. */

	return current_srcfile ? 1 : 0;
}

/*
 * The empty source position.
 */

srcpos srcpos_empty = {
	.first_line = 0,
	.first_column = 0,
	.last_line = 0,
	.last_column = 0,
	.file = NULL,
};

srcpos *
srcpos_copy(srcpos *pos)
{
	srcpos *pos_new;

	pos_new = xmalloc(sizeof(srcpos));
	memcpy(pos_new, pos, sizeof(srcpos));

	return pos_new;
}



void
srcpos_dump(srcpos *pos)
{
	printf("file        : \"%s\"\n",
	       pos->file ? (char *) pos->file : "<no file>");
	printf("first_line  : %d\n", pos->first_line);
	printf("first_column: %d\n", pos->first_column);
	printf("last_line   : %d\n", pos->last_line);
	printf("last_column : %d\n", pos->last_column);
	printf("file        : %s\n", pos->file->name);
}


char *
srcpos_string(srcpos *pos)
{
	const char *fname;
	char col_buf[100];
	char *pos_str;

	if (!pos) {
		fname = "<no-file>";
	} else if (pos->file->name) {
		fname = pos->file->name;
		if (strcmp(fname, "-") == 0)
			fname = "stdin";
	} else {
		fname = "<no-file>";
	}

	if (pos->first_line == pos->last_line) {
		if (pos->first_column == pos->last_column) {
			snprintf(col_buf, sizeof(col_buf),
				 "%d:%d",
				 pos->first_line, pos->first_column);
		} else {
			snprintf(col_buf, sizeof(col_buf),
				 "%d:%d-%d",
				 pos->first_line,
				 pos->first_column, pos->last_column);
		}

	} else {
		snprintf(col_buf, sizeof(col_buf),
			 "%d:%d - %d:%d",
			 pos->first_line, pos->first_column,
			 pos->last_line, pos->last_column);
	}

	if (asprintf(&pos_str, "%s %s", fname, col_buf) == -1)
		return "<unknown source position?";

	return pos_str;
}


void
srcpos_error(srcpos *pos, char const *fmt, ...)
{
	const char *srcstr;
	va_list va;
	va_start(va, fmt);

	srcstr = srcpos_string(pos);

	fprintf(stderr, "Error: %s ", srcstr);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");

	va_end(va);
}


void
srcpos_warn(srcpos *pos, char const *fmt, ...)
{
	const char *srcstr;
	va_list va;
	va_start(va, fmt);

	srcstr = srcpos_string(pos);

	fprintf(stderr, "Warning: %s ", srcstr);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");

	va_end(va);
}
