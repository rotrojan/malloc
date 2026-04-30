/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   free.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 17:38:36 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/22 10:53:36 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FREE_H
#define FREE_H

#include "zone.h"

s_zone_hdr *find_zone(void *ptr);
void        free_tiny(void *ptr, s_zone_hdr *zone_hdr);

#endif /* FREE_H */
