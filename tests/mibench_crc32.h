/* +++Date last modified: 05-Jul-1997 */

/*
**  CRC.H - header file for SNIPPETS CRC and checksum functions
*/

#ifndef CRC__H
#define CRC__H

#include <stdlib.h>           /* For size_t                 */
#include "mibench_sniptype.h"         /* For BYTE, WORD, DWORD      */

/*
**  File: CRC_32.C
*/

#define UPDC32(octet,crc) (crc_32_tab[((crc)^((BYTE)octet)) & 0xff] ^ ((crc) >> 8))

WORD updateCRC32(unsigned char ch, WORD crc);
Boolean_T crc32file(const char *name, unsigned long pcm_size, WORD *crc, unsigned long *charcnt);
WORD crc32buf(char *buf, size_t len);

/*
**  File: CHECKSUM.C
*/

unsigned checksum(void *buffer, size_t len, unsigned int seed);

/*
**  File: CHECKEXE.C
*/

void checkexe(char *fname);

#endif /* CRC__H */
