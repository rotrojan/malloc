# Implicit variables
CC = cc
CFLAGS = -Wall -Wextra -Werror -fPIC $(INCLUDE_DIRS:%=-I%) -MMD -MP
LDFLAGS = -shared -L$(LIBFT) -$(patsubst lib%,l%,$(LIBFT)) -Wl,--wrap=mmap -Wl,--wrap=munmap -lpthread
AR = ar
ARFLAGS = rcs
RM = rm -fr

# Files
SRCS = malloc.c tiny.c small.c large.c zone.c arena.c show_alloc_mem.c bitmap.c stat_mmap.c
OBJS = $(SRCS:%.c=$(CACHE_DIR)/%.o)
DEPS = $(SRCS:%.c=$(CACHE_DIR)/%.d)

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

vpath %.c $(shell find $(SRCS_DIR) -type d)

all: $(NAME) $(SYMLINK)

$(NAME): $(OBJS) $(LIBFT)/$(LIBFT).a
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
	$(RM) $(CACHE_DIR) $(NAME) $(SYMLINK) $(TEST_BIN)
	$(MAKE) -C $(LIBFT) $@

re: fclean all

$(TEST_BIN): test.c $(LIBFT)/$(LIBFT).a
	$(CC) -Wall -Wextra -Werror -g3 -Iinclude -Ilibft/include $^ -Llibft -lft -Wl,-rpath,'$$ORIGIN' -o $@

run_test: $(NAME) $(TEST_BIN)
	LD_PRELOAD=./$(NAME) ./test

.PHONY: all symlink clean fclean re run_test 
