CC = cc
CFLAGS = -Wall -Wextra -Werror -I.
AR = ar
ARFLAGS = rcs
SRCS = malloc.c malloc_tiny.c bitmap.c
OBJS = $(SRCS:%.c=$(OBJSDIR)/%.o)
SRCSDIR = srcs
OBJSDIR = .objs
MKDIR = mkdir -p
RM = rm -fr

ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

NAME = libft_malloc_$(HOSTTYPE).so

vpath %.c $(shell find $(SRCSDIR) -type d)

all: $(NAME)

$(NAME): $(OBJS)
	$(AR) $(ARFLAGS) $(NAME) $^

$(OBJSDIR):
	$(MKDIR) $@

$(OBJSDIR)/%.o: %.c | $(OBJSDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJSDIR)

fclean: clean
	$(RM) $(NAME) test

re: fclean all

test: test.c $(NAME)
	# $(CC) -Wall -Wextra -Werror -fsanitize=address -g3 $^ -o $@
	$(CC) -Wall -Wextra -Werror -g3 -L. $< -o $@

run_test: test
	# LD_PRELOAD=./$(NAME),$(shell $(CC) -print-file-name=libasan.so) ./test
	LD_PRELOAD=./$(NAME) ./test
	

.PHONY: all clean fclean re run_test
