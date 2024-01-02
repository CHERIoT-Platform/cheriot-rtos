#include <string.h>

size_t strlcpy(char *dest, const char *src, size_t n)
{
	size_t copySize = n == 0 ? 0 : n - 1;
	size_t i;

	// Copy up to n - 1 characters from the source string to the destination
	for (i = 0; i < copySize && src[i] != '\0'; i++)
	{
		dest[i] = src[i];
	}
	// If the size is not 0, add a null terminator to the end of the string
	if (n != 0)
	{
		dest[i] = '\0';
	}
	return i;
}
