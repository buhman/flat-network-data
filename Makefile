.POSIX:

include config.mk

SRC = flat-network-data.c
OBJ = $(SRC:.c=.o)
NAME = flat-network-data

all: $(NAME)

%.o: %.c
	@echo CC -c $<
	@$(CC) $(CFLAGS) -c $<

$(NAME): $(OBJ)
	@echo CC -o $@
	@$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(NAME) $(OBJ)

.PHONY: all clean
