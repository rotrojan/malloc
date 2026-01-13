# Implicit variables
CC = cc
CFLAGS = -Wall -Wextra -Werror -fPIC $(INCLUDE_DIRS:%=-I%) -MMD -MP
LDFLAGS = -shared
AR = ar
ARFLAGS = rcs
RM = rm -fr

# Files
SRCS = malloc.c malloc_tiny.c bitmap.c
OBJS = $(SRCS:%.c=$(CACHE_DIR)/%.o)
DEPS = $(SRCS:%.c=$(CACHE_DIR)/%.d)

# Directories
SRCS_DIR = srcs
INCLUDE_DIRS = include $(SRCS_DIR) $(SRCS_DIR)/inc
CACHE_DIR = .cache

# Programs
MKDIR = mkdir -p
FORMAT = clang-format -i

ifdef DEBUG
CFLAGS += -g3
endif

ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

NAME = libft_malloc_$(HOSTTYPE).so

vpath %.c $(shell find $(SRCS_DIR) -type d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(CACHE_DIR):
	$(MKDIR) $@

$(CACHE_DIR)/%.o: %.c | $(CACHE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(CACHE_DIR)

fclean: clean
	$(RM) $(NAME) test

re: fclean all

vpath %.h $(shell find $(SRCS_DIR) -type d) $(INCLUDE_DIRS)
INCS = _malloc.h zone_type.h $(SRCS:%.c=%.h)

norm: $(SRCS) $(INCS)
	$(FORMAT) $^

test: test.c $(NAME)
	# $(CC) -Wall -Wextra -Werror -fsanitize=address -g3 $^ -o $@
	$(CC) -Wall -Wextra -Werror -g3 -L. $< -o $@

run_test: test
	# LD_PRELOAD=./$(NAME),$(shell $(CC) -print-file-name=libasan.so) ./test
	LD_PRELOAD=./$(NAME) ./test


.PHONY: all clean fclean re run_test norm
