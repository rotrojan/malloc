# Implicit variables
CC = cc
CFLAGS = -Wall -Wextra -Werror -fPIC $(INCLUDE_DIRS:%=-I%) -MMD -MP
LDFLAGS = -shared -L$(LIBFT) -$(patsubst lib%,l%,$(LIBFT)) -Wl,--wrap=mmap -Wl,--wrap=munmap -Wl,--version-script=$(VERSION_MAP) -lpthread
AR = ar
ARFLAGS = rcs
RM = rm -fr

# Files
SRCS = malloc.c tiny.c small.c large.c zone.c arena.c show_alloc_mem.c bitmap.c stat_mmap.c
OBJS = $(SRCS:%.c=$(CACHE_DIR)/%.o)
DEPS = $(SRCS:%.c=$(CACHE_DIR)/%.d)
VERSION_MAP = libft_malloc.map

# Directories
SRCS_DIR = srcs
INCLUDE_DIRS = include $(SRCS_DIR) $(SRCS_DIR)/inc $(LIBFT:%=%/include)
CACHE_DIR = .cache

# Programs
MKDIR = mkdir -p
FORMAT = clang-format -i
LINK_PRG = ln -sf

LIBFT = libft

ifdef DEBUG
CFLAGS += -g3
endif

ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

# Bin / link names
NAME = libft_malloc_$(HOSTTYPE).so
SYMLINK = libft_malloc.so
TEST_BIN = test
TEST_MT_BIN = test_mt
TEST_OOM_BIN = test_oom

vpath %.c $(shell find $(SRCS_DIR) -type d)

all: $(NAME) $(SYMLINK)

$(NAME): $(OBJS) $(LIBFT)/$(LIBFT).a $(VERSION_MAP)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

symlink: $(SYMLINK)

$(SYMLINK): $(NAME)
	$(LINK_PRG) $< $@

$(CACHE_DIR):
	$(MKDIR) $@

$(CACHE_DIR)/%.o: %.c | $(CACHE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBFT)/$(LIBFT).a: FORCE
	$(MAKE) -C $(LIBFT)

FORCE:

clean:
	$(RM) $(CACHE_DIR)
	$(MAKE) -C $(LIBFT) $@

fclean:
	$(RM) $(CACHE_DIR) $(NAME) $(SYMLINK) $(TEST_BIN) $(TEST_MT_BIN) $(TEST_OOM_BIN)
	$(MAKE) -C $(LIBFT) $@

re: fclean all

$(TEST_BIN): test.c $(LIBFT)/$(LIBFT).a
	$(CC) -Wall -Wextra -Werror -g3 -Iinclude -Ilibft/include $^ -Llibft -lft -Wl,-rpath,'$$ORIGIN' -o $@

run_test: $(NAME) $(TEST_BIN)
	LD_PRELOAD=./$(SYMLINK) ./test

$(TEST_MT_BIN): test_mt.c
	$(CC) -Wall -Wextra -Werror -g3 -D_GNU_SOURCE -pthread $< -o $@

# Multithreaded stress test under helgrind. --soname-synonyms=somalloc=NONE
# stops valgrind from replacing malloc with its own, so OUR allocator runs and
# helgrind can analyse its locking.
run_test_mt: $(NAME) $(TEST_MT_BIN)
	LD_PRELOAD=./$(SYMLINK) valgrind --tool=helgrind \
		--soname-synonyms=somalloc=NONE --error-exitcode=1 ./$(TEST_MT_BIN)

$(TEST_OOM_BIN): test_oom.c
	$(CC) -Wall -Wextra -Werror -g3 $< -o $@

# Out-of-memory test: lowers RLIMIT_AS and checks malloc fails gracefully
# (returns NULL, no crash) at the address-space ceiling, then recovers on free.
run_test_oom: $(NAME) $(TEST_OOM_BIN)
	LD_PRELOAD=./$(SYMLINK) ./$(TEST_OOM_BIN)

.PHONY: all symlink clean fclean re run_test run_test_mt run_test_oom
