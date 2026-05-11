/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   magazine.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/16 12:58:34 by rotrojan          #+#    #+#             */
/*   Updated: 2026/05/11 14:50:33 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "magazine.h"
#include "malloc_state.h"

#include "libft.h"

#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
;
s_thread_local_storage g_thread_local_storage = { .once_control =
							  PTHREAD_ONCE_INIT };

static void destroy_magazine(void *mag)
{
	int ret = munmap(mag, PAGE_SIZE);
	if (ret == -1)
		ft_dprintf(STDERR_FILENO, "Fatal: cannot destroy magazine!\n");
}

static void create_keys(void)
{
	/* TODO: What if I reach PTHREAD_KEYS_MAX? */
	pthread_key_create(&g_thread_local_storage.key, &destroy_magazine);
}

static s_magazine *new_magazine(void)
{
	s_magazine *mag =
		(s_magazine *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mag == MAP_FAILED) {
		ft_dprintf(STDERR_FILENO, "Fatal: cannot create magazine!\n");
		return NULL;
	}

	pthread_setspecific(g_thread_local_storage.key, mag);

	ft_memset(mag, 0, sizeof(*mag));

	return mag;
}

s_magazine *get_magazine(void)
{
	s_magazine *mag = NULL;

	pthread_once(&g_thread_local_storage.once_control, &create_keys);

	mag = pthread_getspecific(g_thread_local_storage.key);

	if (mag == NULL)
		mag = new_magazine();

	return mag;
}
