/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_printf.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/23 16:17:46 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/22 16:23:57 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define BUF_SIZE 1024

typedef struct {
	char   buf[BUF_SIZE];
	size_t len;
	int    fd;
} s_buffer;

static void buffer_flush(s_buffer *buffer)
{
	if (buffer->len > 0) {
		write(buffer->fd, buffer->buf, buffer->len);
		buffer->len = 0;
	}
}

static void buffer_putchar(s_buffer *buffer, char c)
{
	if (buffer->len == BUF_SIZE)
		buffer_flush(buffer);
	buffer->buf[buffer->len++] = c;
}

static void buffer_putstr(s_buffer *buffer, const char *s)
{
	if (!s)
		s = "(null)";
	while (*s)
		buffer_putchar(buffer, *s++);
}

static void buffer_putdecimal(s_buffer *buffer, int n)
{
	char         tmp[12];
	int          i = 11;
	unsigned int u;

	tmp[i] = '\0';

	if (n < 0) {
		buffer_putchar(buffer, '-');
		u = -(unsigned int)n; /* -(unsigned)INT_MIN is well-defined */
	} else
		u = (unsigned int)n;

	if (u == 0)
		return buffer_putchar(buffer, '0');

	while (u > 0) {
		tmp[--i] = '0' + (u % 10);
		u /= 10;
	}

	while (tmp[i])
		buffer_putchar(buffer, tmp[i++]);
}

static void buffer_putudecimal(s_buffer *buffer, size_t n)
{
	char tmp[21]; /* 2^64 - 1 == 20 digits, + NUL */
	int  i = 20;

	tmp[i] = '\0';

	if (n == 0)
		return buffer_putchar(buffer, '0');

	while (n > 0) {
		tmp[--i] = '0' + (n % 10);
		n /= 10;
	}

	while (tmp[i])
		buffer_putchar(buffer, tmp[i++]);
}

static void buffer_puthex(s_buffer *buffer, unsigned int n, int capital)
{
	char tmp[9];
	int  i = 8;
	int  nibble;
	char a = capital ? 'A' : 'a';

	tmp[i] = '\0';

	if (n == 0) {
		buffer_putchar(buffer, '0');
		return;
	}

	while (n > 0) {
		nibble   = n & 0xF;
		tmp[--i] = nibble < 10 ? '0' + nibble : a + nibble - 10;
		n >>= 4;
	}

	while (tmp[i])
		buffer_putchar(buffer, tmp[i++]);
}

static void buffer_putptr(s_buffer *buffer, void *ptr)
{
	char tmp[2 + sizeof(uintptr_t) * 2 + 1]; /* "0x" + hex digits + NUL */
	uintptr_t n = (uintptr_t)ptr;
	int       i = sizeof(tmp) - 1;
	int       nibble;

	tmp[i] = '\0';

	if (n == 0)
		return buffer_putstr(buffer, "(nil)");

	while (n > 0) {
		nibble   = n & 0xF;
		tmp[--i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
		n >>= 4;
	}
	tmp[--i] = 'x';
	tmp[--i] = '0';

	while (tmp[i])
		buffer_putchar(buffer, tmp[i++]);
}

typedef enum {
	LEN_NONE,
	LEN_Z, /* 'z' modifier: argument is size_t-wide */
} e_length;

static void ft_vdprintf(int fd, char const *fmt, va_list ap)
{
	s_buffer buffer;

	buffer.fd  = fd;
	buffer.len = 0;

	while (*fmt) {
		if (*fmt != '%') {
			buffer_putchar(&buffer, *fmt++);
			continue;
		}

		fmt++; /* skip '%' */

		e_length length = LEN_NONE;
		if (*fmt == 'z') {
			length = LEN_Z;
			fmt++;
		}

		switch (*fmt) {
		case 's':
			buffer_putstr(&buffer, va_arg(ap, const char *));
			break;
		case 'd':
			buffer_putdecimal(&buffer, va_arg(ap, int));
			break;
		case 'u':
			if (length == LEN_Z)
				buffer_putudecimal(&buffer, va_arg(ap, size_t));
			else
				buffer_putudecimal(&buffer,
						   va_arg(ap, unsigned int));
			break;
		case 'x':
			buffer_puthex(&buffer, va_arg(ap, unsigned int), 0);
			break;
		case 'X':
			buffer_puthex(&buffer, va_arg(ap, unsigned int), 1);
			break;
		case 'p':
			buffer_putptr(&buffer, va_arg(ap, void *));
			break;
		case '%':
			buffer_putchar(&buffer, '%');
			break;
		default:
			buffer_putchar(&buffer, '%');
			buffer_putchar(&buffer, *fmt);
			break;
		}
		fmt++;
	}

	buffer_flush(&buffer);
}

void ft_dprintf(int fd, char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ft_vdprintf(fd, fmt, ap);
	va_end(ap);
}

void ft_printf(char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ft_vdprintf(STDOUT_FILENO, fmt, ap);
	va_end(ap);
}
