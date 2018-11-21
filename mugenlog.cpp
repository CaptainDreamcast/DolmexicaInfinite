#include "mugenlog.h"

#include <stdio.h>
#include <stdarg.h>

#include "gamelogic.h"

void mugf(char * tFormatString, ...)
{
	char text[1024];
	va_list args;
	va_start(args, tFormatString);
	vsprintf(text, tFormatString, args);
	va_end(args);

	printf("%d %s\n", getDreamGameTime(), text);
}
