# Implicit variables
CC = cc
CFLAGS = -Wall -Wextra -Werror -I$(INCLUDE_DIR) -MMD -MP -fPIC
AR = ar
ARFLAGS = rcs
RM = rm -fr

# Files
SRCS = \
  ft_memset.c \
  ft_memcpy.c \
  ft_printf.c
#   ft_bzero.c \
#   ft_memcpy.c \
#   ft_memccpy.c \
#   ft_memmove.c \
#   ft_memchr.c \
#   ft_memcmp.c \
#   ft_isalpha.c \
#   ft_isdigit.c \
#   ft_isalnum.c \
#   ft_isascii.c \
#   ft_isprint.c \
#   ft_toupper.c \
#   ft_tolower.c \
#   ft_strchr.c \
#   ft_strrchr.c \
#   ft_strncmp.c \
#   ft_strlcpy.c \
#   ft_strlcat.c \
#   ft_strnstr.c \
#   ft_atoi.c \
#   ft_strdup.c \
#   ft_calloc.c \
#   ft_substr.c \
#   ft_strjoin.c \
#   ft_strtrim.c \
#   ft_split.c \
#   ft_itoa.c \
#   ft_strmapi.c \
#   ft_putchar_fd.c \
#   ft_putendl_fd.c \
#   ft_putnbr_fd.c
OBJS = $(SRCS:%.c=$(CACHE_DIR)/%.o)
DEPS = $(SRCS:%.c=$(CACHE_DIR)/%.d)

# Directories
SRCS_DIR = srcs
INCLUDE_DIR = include
CACHE_DIR = .cache


# Programs
MKDIR = mkdir -p

NAME = libft.a

vpath %.c $(SRCS_DIR)

all: $(NAME)

$(NAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

-include $(DEPS)
$(CACHE_DIR)/%.o: %.c | $(CACHE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


$(CACHE_DIR):
	$(MKDIR) $@

clean:
	$(RM) $(CACHE_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: re all clean fclean
