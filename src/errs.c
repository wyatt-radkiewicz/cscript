#include <stdio.h>

#include "common.h"

static void getsrcloc(cnm_t *cnm, const char *pt, int *line, int *col,
			char *lnbuf, int lnbufsz) {
	*line = 1, *col = 1;
	const char *lineptr = cnm->src;
	for (const char *i = cnm->src; i < pt; ++i, ++*col) {
		if (*i == '\n') {
			++*line;
			*col = 0;
			lineptr = i+1;
		}
	}

	const char *lineend = strchr(lineptr, '\n');
	if (!lineend) lineend = lineptr + strlen(lineptr);
	int cpysz = min(lineend - lineptr, lnbufsz - 1);
	memcpy(lnbuf, lineptr, cpysz);
	lnbuf[cpysz] = '\0';
}

void doerr(cnm_t *cnm, int code, const char *desc,
		bool iserr, errinf_t *infs, int ninfs) {
	cnm->err.diderr |= iserr;
	cnm->err.nerrs += iserr;
	if (!cnm->err.cb) return;
	
	int loc, col, line, start_line;
	char lnbuf[256];

	{
		strview_t area = infs ? infs[0].area: cnm->tok.src;
		getsrcloc(cnm, area.str, &line, &col,
			lnbuf, sizeof(lnbuf));

		loc = snprintf(cnm->err.pbuf, sizeof(cnm->err.pbuf),
			"%s[%c%04d]: %s\n"
			"  --> %s:%d:%d\n",
			iserr ? "error" : "warning",
			iserr ? 'E' : 'W',
			code, desc, cnm->filename, line, col);

		start_line = line;
	}

	for (int i = 0; infs;) {
		int oldline;

		if (loc >= sizeof(cnm->err.pbuf) - 1)
			goto prnterr;
		
		loc += snprintf(cnm->err.pbuf + loc, sizeof(cnm->err.pbuf) - loc,
			"   |\n"
			"%-3d|%s\n",
			line, lnbuf);

		if (infs[i].comment[0] == '\0')
			goto skip_comment;

		// Now add the comment
		if (loc + col + 6 + infs[i].area.len + strlen(infs[i].comment)
			> sizeof(cnm->err.pbuf))
			goto prnterr;

		memcpy(cnm->err.pbuf + loc, "   |", 4);
		loc += 4;
		memset(cnm->err.pbuf + loc, ' ', col - 1);
		loc += col - 1;
		memset(cnm->err.pbuf + loc, infs[i].critical ? '~' : '-', infs[i].area.len);
		loc += infs[i].area.len;
		cnm->err.pbuf[loc++] = ' ';
		memcpy(cnm->err.pbuf + loc, infs[i].comment, strlen(infs[i].comment));
		loc += strlen(infs[i].comment);
		cnm->err.pbuf[loc++] = '\n';
		cnm->err.pbuf[loc++] = '\0';

		if (++i == ninfs) break;

skip_comment:
		oldline = line;

		getsrcloc(cnm, infs[i].area.str, &line, &col,
			lnbuf, sizeof(lnbuf));

		// Add a ...
		if (line - oldline > 1) {
			if (loc + strlen("   |\n...\n") >=
				sizeof(cnm->err.pbuf) - 1)
				goto prnterr;
			strcat(cnm->err.pbuf, "   |\n...\n");
		}
	}

prnterr:
	cnm->err.cb(cnm->err.pbuf, desc, start_line);
}

