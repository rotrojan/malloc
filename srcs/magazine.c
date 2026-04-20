/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   magazine.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/16 12:58:34 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/20 14:12:29 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "magazine.h"
#include "malloc_state.h"

#include "libft.h"

#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAGAZINE_SIZE (sysconf(_SC_PAGESIZE))

s_thread_local_storage g_thread_local_storage = { .once_control =
							  PTHREAD_ONCE_INIT };

static void destroy_magazine(void *mag)
{
	munmap(mag, MAGAZINE_SIZE);
}

static void create_keys(void)
{
	/* TODO: What if I reach PTHREAD_KEYS_MAX? */
	pthread_key_create(&g_thread_local_storage.key, &destroy_magazine);
}

/*
static s_magazine *pop_magazine(s_magazine **mag_list, s_magazine *mag)
{
	s_magazine **current = mag_list;

	while (*current != NULL) {
		if (*current == mag) {
			*current = mag->next;
			mag->next = NULL;
			return mag;
		}
		current = &((*current)->next);
	}

	return NULL;
}
*/

static s_magazine *new_magazine(void)
{
	s_magazine *mag = NULL;

	if (g_malloc_state.current_memory + MAGAZINE_SIZE >
	    g_malloc_state.max_memory)
		return NULL;

	mag = (s_magazine *)mmap(NULL, MAGAZINE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	g_malloc_state.current_memory += MAGAZINE_SIZE;
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
