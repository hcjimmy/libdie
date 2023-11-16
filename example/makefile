CC = gcc -lm

SRC_DIRS = . .. ../list
OBJS = $(patsubst %.c, %.o, $(foreach fol, $(SRC_DIRS), $(wildcard $(fol)/*.c)))

example: $(OBJS)
	$(CC) -o $@ $^

clean: ; rm $(OBJS)
