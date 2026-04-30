#include "libft.h"
#include "srcs/show_alloc_mem.h"
#include "malloc.h"

void show_alloc_mem(void) __attribute__((weak));

static inline void *malloc_and_show(char name[], size_t size)
{
	void *ptr = malloc(size);
	ft_printf("%s = %p (%d bytes)\n", name, ptr, size);

	return ptr;
}

int main(void)
{
	if (1) {

		void *ptr_array[12560];
		for (size_t i = 0; i < sizeof(ptr_array) / sizeof(*ptr_array); i++)
		{
			ptr_array[i] = malloc(16);
			if (i == 255 || i == 256)
				ft_printf("%d: %p\n", i, ptr_array[i]);
		}

		if (show_alloc_mem) {
			ft_printf("\nSHOW MEM ALLOC\n");
			show_alloc_mem();
		}

		for (size_t i = 0; i < sizeof(ptr_array) /sizeof(*ptr_array); i++) {
			/* ft_printf("Freeing ptr[%d]\n", i); */
			free(ptr_array[i]);
		}

		if (show_alloc_mem) {
			ft_printf("\nSHOW MEM ALLOC\n");
			show_alloc_mem();
		}
	}

	if (0) {
		void *ptr1 = malloc_and_show("ptr1", 120);
		void *ptr2 = malloc_and_show("ptr2", 64);
		void *ptr3 = malloc_and_show("ptr3", 16);
		void *ptr4 = malloc_and_show("ptr4", 17);
		void *ptr5 = malloc_and_show("ptr5", 100);
		void *ptr6 = malloc_and_show("ptr6", 100);

		if (show_alloc_mem) {
			ft_printf("\nSHOW MEM ALLOC\n");
			show_alloc_mem();
		}

		free(ptr1);
		ft_printf("ptr1 freed\n");
		free(ptr3);
		ft_printf("ptr3 freed\n");
		free(ptr5);
		ft_printf("ptr5 freed\n");

		if (show_alloc_mem) {
			ft_printf("\nSHOW MEM ALLOC\n");
			show_alloc_mem();
		}

		ptr1 = malloc_and_show("ptr1", 12);
		ptr3 = malloc_and_show("ptr3", 90);
		ptr5 = malloc_and_show("ptr5", 12);

		if (show_alloc_mem) {
			ft_printf("\nSHOW MEM ALLOC\n");
			show_alloc_mem();
		}

		free(ptr1);
		free(ptr2);
		free(ptr3);
		free(ptr4);
		free(ptr5);
		free(ptr6);

		if (show_alloc_mem) {
			ft_printf("\nSHOW MEM ALLOC\n");
			show_alloc_mem();
		}
	}

	return 0;
}
