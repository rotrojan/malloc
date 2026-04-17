#include "malloc.h"

#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

static void print_error(char const *str)
{
	int i = 0;

	while (str[i])
		i++;
	write(1, str, i);
	write(1, "\n", 1);
}

int main()
{
	void *handle = dlopen("./libft_malloc_x86_64_Linux.so", RTLD_NOW);
	char *error = dlerror();

	if (error)
	{
		print_error(error);
		return -1;
	}

	void *(*my_malloc)(size_t) = dlsym(handle, "malloc");

	/* void *ptr_0 = malloc(0); */
	void *ptr_1 = my_malloc(42);
	void *ptr_2 = my_malloc(12);
	void *ptr_3 = my_malloc(234);
	void *ptr_4 = my_malloc(256);

	dlclose(handle);

	/* printf("ptr_0 = %p\n", ptr_0); */
	/* printf("size = %ld\n", ptr_1 - ptr_0); */
	printf("ptr_1 = %p\n", ptr_1);
	printf("size = %ld\n", ptr_2 - ptr_1);
	printf("ptr_2 = %p\n", ptr_2);
	printf("size = %ld\n", ptr_3 - ptr_2);
	printf("ptr_3 = %p\n", ptr_3);
	printf("size = %ld\n", ptr_4 - ptr_3);
	printf("ptr_4 = %p\n", ptr_4);
	/* printf("ptr_1 = %p\n", ptr_1); */
	/* printf("size = %ld\n", ptr_2 - ptr_1); */
	/* printf("ptr_2 = %p\n", ptr_2); */
	/* printf("size = %ld\n", ptr_3 - ptr_2); */
	/* printf("ptr_3 = %p\n", ptr_3); */
	/* printf("size = %ld\n", ptr_4 - ptr_3); */
	/* printf("ptr_4 = %p\n", ptr_4); */

	return 0;
}
