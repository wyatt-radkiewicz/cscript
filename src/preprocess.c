#include <string.h>

#include "preprocess.h"

size_t preprocess(const char *src, char *buf, size_t buflen) {
	size_t len = strlen(src);
	if (len > buflen) len = buflen;
	memcpy(buf, src, len);
	buf[len == buflen ? buflen - 1 : len] = '\0';
	return len;
}

