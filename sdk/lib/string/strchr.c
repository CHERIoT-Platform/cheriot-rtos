#include <string.h>

char *strchr(const char *s, int intChar)
{
	char c = (char)intChar;
	while (*s != c)
	{
		if (!*s++)
		{
			return NULL;
		}
	}
	return (char *)s;
}
