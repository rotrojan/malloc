# Implicit variables
CC = cc
CFLAGS = -Wall -Wextra -Werror -fPIC $(INCLUDE_DIRS:%=-I%) -MMD -MP
LDFLAGS = -shared -L$(LIBFT) -$(patsubst lib%,l%,$(LIBFT))
AR = ar
ARFLAGS = rcs
RM = rm -fr

# Files
SRCS = malloc.c malloc_tiny.c bitmap.c
OBJS = $(SRCS:%.c=$(CACHE_DIR)/%.o)
DEPS = $(SRCS:%.c=$(CACHE_DIR)/%.d)

# Directories
SRCS_DIR = srcs
INCLUDE_DIRS = include $(SRCS_DIR) $(SRCS_DIR)/inc $(LIBFT:%=%/include)
CACHE_DIR = .cache

# Programs
MKDIR = mkdir -p
FORMAT = clang-format -i

LIBFT = libft

ifdef DEBUG
CFLAGS += -g3
endif

ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

NAME = libft_malloc_$(HOSTTYPE).so
TEST_BIN = test

vpath %.c $(shell find $(SRCS_DIR) -type d)

all: $(NAME)

$(NAME): $(OBJS) $(LIBFT)/$(LIBFT).a
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(CACHE_DIR):
	$(MKDIR) $@

$(CACHE_DIR)/%.o: %.c | $(CACHE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBFT)/$(LIBFT).a: FORCE
	$(MAKE) -C $(LIBFT)

FORCE:

clean:
	$(RM) $(CACHE_DIR)
	make -C $(LIBFT) $@

fclean:
	$(RM) $(CACHE_DIR) $(NAME) $(TEST_BIN)
	make -C $(LIBFT) $@

re: fclean all

vpath %.h $(shell find $(SRCS_DIR) -type d) $(INCLUDE_DIRS)
INCS = _malloc.h zone_type.h $(SRCS:%.c=%.h)

norm: $(SRCS) $(INCS)
	$(FORMAT) $^

$(TEST_BIN): test.c
	$(CC) -Wall -Wextra -Werror -g3 $^ -o $@

run_test: $(NAME) $(TEST_BIN)
	LD_PRELOAD=./$(NAME) ./test

.PHONY: all clean fclean re run_test norm
