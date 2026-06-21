/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memcpy.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/14 14:56:51 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/11 15:39:04 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void *ft_memcpy(void *dst, void const *src, size_t n)
{
	unsigned char       *d;
	unsigned const char *s;

	d = dst;
	s = src;

	for (size_t i = 0; i < n; i++)
		d[i] = s[i];

	return (dst);
}
