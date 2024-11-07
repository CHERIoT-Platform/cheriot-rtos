/*-
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_prf.c	8.3 (Berkeley) 1/21/94
 */

/* __FBSDID("$FreeBSD: stable/8/sys/kern/subr_prf.c 210305 2010-07-20 18:55:13Z
 * jkim $"); */

#include <cdefs.h>
#include <cheri-builtins.h>
#include <compartment.h>
#include <cstdint>
#include <function_wrapper.hh>
#include <inttypes.h>
#include <platform-uart.hh>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * Definitions snarfed from various parts of the FreeBSD headers:
 */
namespace
{
	__always_inline char toupper(char c)
	{
		return ((c)-0x20 * (((c) >= 'a') && ((c) <= 'z')));
	}

	static char hex2ascii(uintmax_t in)
	{
		return (in < 10) ? '0' + in : 'a' + in - 10;
	}

/* Max number conversion buffer length: a u_quad_t in base 2, plus NUL byte. */
#define MAXNBUF (sizeof(intmax_t) * CHAR_BIT + 1)

	/*
	 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
	 * order; return an optional length and a pointer to the last character
	 * written in the buffer (i.e., the first character of the string).
	 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
	 */
	// FIXME: Using `unsigned` for `num` instead of `uintmax_t` means that we
	// are going to truncate large numbers, but it avoids needing a library
	// routine to handle division.
	static char *
	ksprintn(char *nbuf, unsigned num, int base, int *lenp, int upper)
	{
		char *p, c;

		p  = nbuf;
		*p = '\0';
		do
		{
			c    = hex2ascii(num % base);
			*++p = upper ? toupper(c) : c;
		} while (num /= base);
		if (lenp)
		{
			*lenp = p - nbuf;
		}
		return (p);
	}

	/*
	 * Scaled down version of printf(3).
	 *
	 * Two additional formats:
	 *
	 * The format %b is supported to decode error registers.
	 * Its usage is:
	 *
	 *	printf("reg=%b\n", regval, "<base><arg>*");
	 *
	 * where <base> is the output base expressed as a control character, e.g.
	 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
	 * the first of which gives the bit number to be inspected (origin 1), and
	 * the next characters (up to a control character, i.e. a character <= 32),
	 * give the name of the register.  Thus:
	 *
	 *	kvprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
	 *
	 * would produce output:
	 *
	 *	reg=3<BITTWO,BITONE>
	 *
	 * %D  -- Hexdump, takes pointer and separator string:
	 *		("%6D", ptr, ":")   -> XX:XX:XX:XX:XX:XX
	 *		("%*D", len, ptr, " " -> XX XX XX XX ...
	 */
	__noinline int kvprintf(char const                *fmt,
	                        FunctionWrapper<void(int)> func,
	                        void                      *arg,
	                        int                        radix,
	                        va_list                    ap)
	{
		char           nbuf[MAXNBUF];
		const char    *p, *percent, *q;
		unsigned char *up;
		int            ch, n;
		uintmax_t      num;
		int  base, lflag, qflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
		int  cflag, hflag, jflag, tflag, zflag;
		int  dwidth, upper;
		char padc;
		int  stop = 0, retval = 0;
		const char *emptyString = "";

		auto putchar = [&](int c) {
			func(c);
			retval++;
		};

		if (fmt == nullptr)
		{
			fmt = emptyString;
		}

		if (radix < 2 || radix > 36)
		{
			radix = 10;
		}

		for (;;)
		{
			padc  = ' ';
			width = 0;
			while ((ch = static_cast<unsigned char>(*fmt++)) != '%' || stop)
			{
				if (ch == '\0')
				{
					return (retval);
				}
				putchar(ch);
			}
			percent   = fmt - 1;
			qflag     = 0;
			lflag     = 0;
			ladjust   = 0;
			sharpflag = 0;
			neg       = 0;
			sign      = 0;
			dot       = 0;
			dwidth    = 0;
			upper     = 0;
			cflag     = 0;
			hflag     = 0;
			jflag     = 0;
			tflag     = 0;
			zflag     = 0;
		reswitch:
			switch (ch = static_cast<unsigned char>(*fmt++))
			{
				case '.':
					dot = 1;
					goto reswitch; // NOLINT
				case '#':
					sharpflag = 1;
					goto reswitch; // NOLINT
				case '+':
					sign = 1;
					goto reswitch; // NOLINT
				case '-':
					ladjust = 1;
					goto reswitch; // NOLINT
				case '%':
					putchar(ch);
					break;
				case '*':
					if (!dot)
					{
						width = va_arg(ap, int);
						if (width < 0)
						{
							ladjust = !ladjust;
							width   = -width;
						}
					}
					else
					{
						dwidth = va_arg(ap, int);
					}
					goto reswitch; // NOLINT
				case '0':
					if (!dot)
					{
						padc = '0';
						goto reswitch; // NOLINT
					}
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					for (n = 0;; ++fmt)
					{
						n  = n * 10 + ch - '0';
						ch = *fmt;
						if (ch < '0' || ch > '9')
						{
							break;
						}
					}
					if (dot)
					{
						dwidth = n;
					}
					else
					{
						width = n;
					}
					goto reswitch; // NOLINT
				case 'b':
					num = static_cast<unsigned int>(va_arg(ap, int));
					p   = va_arg(ap, char *);
					for (q = ksprintn(nbuf, num, *p++, nullptr, 0); *q;)
					{
						putchar(*q--);
					}

					if (num == 0)
					{
						break;
					}

					for (tmp = 0; *p;)
					{
						n = *p++;
						if (num & (1 << (n - 1)))
						{
							putchar(tmp ? ',' : '<');
							for (; (n = *p) > ' '; ++p)
							{
								putchar(n);
							}
							tmp = 1;
						}
						else
						{
							for (; *p > ' '; ++p)
							{
								continue;
							}
						}
					}
					if (tmp)
					{
						putchar('>');
					}
					break;
				case 'c':
					putchar(va_arg(ap, int));
					break;
				case 'D':
					up = va_arg(ap, unsigned char *);
					p  = va_arg(ap, char *);
					if (!width)
					{
						width = 16;
					}
					while (width--)
					{
						putchar(hex2ascii(*up >> 4));
						putchar(hex2ascii(*up & 0x0f));
						up++;
						if (width)
						{
							for (q = p; *q; q++)
							{
								putchar(*q);
							}
						}
					}
					break;
				case 'd':
				case 'i':
					base = 10;
					sign = 1;
					goto handle_sign; // NOLINT
				case 'h':
					if (hflag)
					{
						hflag = 0;
						cflag = 1;
					}
					else
					{
						hflag = 1;
					}
					goto reswitch; // NOLINT
				case 'j':
					jflag = 1;
					goto reswitch; // NOLINT
				case 'l':
					if (lflag)
					{
						lflag = 0;
						qflag = 1;
					}
					else
					{
						lflag = 1;
					}
					goto reswitch; // NOLINT
				case 'n':
					if (jflag)
					{
						*(va_arg(ap, intmax_t *)) = retval;
					}
					else if (qflag)
					{
						*(va_arg(ap, long long *)) = retval;
					}
					else if (lflag)
					{
						*(va_arg(ap, long *)) = retval;
					}
					else if (zflag)
					{
						*(va_arg(ap, size_t *)) = retval;
					}
					else if (hflag)
					{
						*(va_arg(ap, short *)) = static_cast<short>(retval);
					}
					else if (cflag)
					{
						*(va_arg(ap, char *)) = retval;
					}
					else
					{
						*(va_arg(ap, int *)) = retval;
					}
					break;
				case 'o':
					base = 8;
					goto handle_nosign; // NOLINT
				case 'p':
					base      = 16;
					sharpflag = (width == 0);
					sign      = 0;
					num       = static_cast<size_t>(
                      reinterpret_cast<uintptr_t>(va_arg(ap, void *)));
					goto number; // NOLINT
				case 'q':
					qflag = 1;
					goto reswitch; // NOLINT
				case 'r':
					base = radix;
					if (sign)
					{
						goto handle_sign; // NOLINT
					}
					goto handle_nosign; // NOLINT
				case 's':
					p = va_arg(ap, char *);
					if (p == nullptr)
					{
						p = emptyString;
					}
					if (!dot)
					{
						n = strlen(p);
					}
					else
					{
						for (n = 0; n < dwidth && p[n]; n++)
						{
							continue;
						}
					}

					width -= n;

					if (!ladjust && width > 0)
					{
						while (width--)
						{
							putchar(padc);
						}
					}
					while (n--)
					{
						putchar(*p++);
					}
					if (ladjust && width > 0)
					{
						while (width--)
						{
							putchar(padc);
						}
					}
					break;
				case 't':
					tflag = 1;
					goto reswitch; // NOLINT
				case 'u':
					base = 10;
					goto handle_nosign; // NOLINT
				case 'X':
					upper = 1;
				case 'x':
					base = 16;
					goto handle_nosign; // NOLINT
				case 'y':
					base = 16;
					sign = 1;
					goto handle_sign; // NOLINT
				case 'z':
					zflag = 1;
					goto reswitch; // NOLINT
				handle_nosign:
					sign = 0;
					if (jflag)
					{
						num = va_arg(ap, uintmax_t);
					}
					else if (qflag)
					{
						num = va_arg(ap, unsigned long long);
					}
					else if (tflag)
					{
						num = va_arg(ap, ptrdiff_t);
					}
					else if (lflag)
					{
						num = va_arg(ap, unsigned long);
					}
					else if (zflag)
					{
						num = va_arg(ap, size_t);
					}
					else if (hflag)
					{
						num = static_cast<unsigned short>(va_arg(ap, int));
					}
					else if (cflag)
					{
						num = static_cast<unsigned char>(va_arg(ap, int));
					}
					else
					{
						num = va_arg(ap, unsigned int);
					}
					goto number; // NOLINT
				handle_sign:
					if (jflag)
					{
						num = va_arg(ap, intmax_t);
					}
					else if (qflag)
					{
						num = va_arg(ap, long long);
					}
					else if (tflag)
					{
						num = va_arg(ap, ptrdiff_t);
					}
					else if (lflag)
					{
						num = va_arg(ap, long);
					}
					else if (zflag)
					{
						num = va_arg(ap, ssize_t);
					}
					else if (hflag)
					{
						num = static_cast<short>(va_arg(ap, int));
					}
					else if (cflag)
					{
						num = static_cast<char>(va_arg(ap, int));
					}
					else
					{
						num = va_arg(ap, int);
					}
				number:
					if (sign && static_cast<intmax_t>(num) < 0)
					{
						neg = 1;
						num = -static_cast<intmax_t>(num);
					}
					p   = ksprintn(nbuf, num, base, &n, upper);
					tmp = 0;
					if (sharpflag && num != 0)
					{
						if (base == 8)
						{
							tmp++;
						}
						else if (base == 16)
						{
							tmp += 2;
						}
					}
					if (neg)
					{
						tmp++;
					}

					if (!ladjust && padc == '0')
					{
						dwidth = width - tmp;
					}
					width -= tmp + (dwidth > n ? dwidth : n);
					dwidth -= n;
					if (!ladjust)
					{
						while (width-- > 0)
						{
							putchar(' ');
						}
					}
					if (neg)
					{
						putchar('-');
					}
					if (sharpflag && num != 0)
					{
						if (base == 8)
						{
							putchar('0');
						}
						else if (base == 16)
						{
							putchar('0');
							putchar('x');
						}
					}
					while (dwidth-- > 0)
					{
						putchar('0');
					}

					while (*p)
					{
						putchar(*p--);
					}

					if (ladjust)
					{
						while (width-- > 0)
						{
							putchar(' ');
						}
					}

					break;
				default:
					while (percent < fmt)
					{
						putchar(*percent++);
					}
					/*
					 * Since we ignore an formatting argument it is no
					 * longer safe to obey the remaining formatting
					 * arguments as the arguments will no longer match
					 * the format specs.
					 */
					stop = 1;
					break;
			}
		}
	}

} // namespace

/*
 * Scaled down version of vsnprintf(3).
 */
int __cheri_libcall
vsnprintf(char *str, // NOLINT (clang-tidy spuriously thinks this should be
                     // const, even though it's being written to)
          size_t      size,
          const char *format,
          va_list     ap)
{
	struct Buffer
	{
		char  *str;
		size_t remain;
	} info        = {str, size};
	auto callback = [&](int ch) {
		if (info.remain >= 2)
		{
			*info.str++ = ch;
			info.remain--;
		}
	};
	int retval = kvprintf(format, callback, &info, 10, ap);
	if (info.remain >= 1)
	{
		*info.str++ = '\0';
	}
	return (retval);
}

[[cheri::interrupt_state(disabled)]] int __cheri_libcall
vfprintf(FILE *stream, const char *fmt, va_list ap)
{
	return kvprintf(
	  fmt,
	  [=](int ch) { static_cast<volatile Uart *>(stream)->blocking_write(ch); },
	  nullptr,
	  10,
	  ap);
}

int __cheri_libcall snprintf(char *str, size_t size, const char *format, ...)
{
	int     rv;
	va_list ap;

	va_start(ap, format);
	rv = vsnprintf(str, size, format, ap);
	va_end(ap);

	return rv;
}
