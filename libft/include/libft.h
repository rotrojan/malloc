/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   libft.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/10/08 13:04:05 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/11 15:40:06 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LIBFT_H
#define LIBFT_H

#include <stddef.h> /* for size_t */

void *ft_memset(void *b, int c, size_t len);
void *ft_memcpy(void *dst, void const *src, size_t n);
void  ft_dprintf(int fd, char const *fmt, ...);
void  ft_printf(char const *fmt, ...);

#endif /* LIBFT_H */
