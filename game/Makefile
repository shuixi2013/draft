# Author: Li Zheng <lizheng_w5625@tp-link.net>

CC			= gcc

CFLAGS		+= -Wall
LDFLAGS		+= -lm -lcurses
INCLUDE		+= -I$(ROOT_DIR)

SUBDIRS		= $(shell ls -l | grep ^d | awk '{if($$9 != "out") print $$9}')
ROOT_DIR	= $(shell pwd)
OUT			= $(ROOT_DIR)/out
CUR_SOURCE	= $(wildcard *.c)
CUR_OBJS	= $(patsubst %.c, %.o, $(CUR_SOURCE))
BIN			= game

export CC ROOT_DIR OUT CFLAGS LDFLAGS INCLUDE

.PHONY: all clean install test

all: $(SUBDIRS) $(CUR_OBJS) $(OUT)

$(SUBDIRS): ECHO
	[ -d "$(OUT)" ] || mkdir -p $(OUT)
	make -C $@

$(OUT): ECHO
	$(CC) $(CFLAGS) $(OUT)/*.o -o $(OUT)/$(BIN) $(LDFLAGS) $(INCLUDE)

ECHO:
	@echo $(SUBDIRS)

$(CUR_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $(OUT)/$@ $(LDFLAGS) $(INCLUDE)

test:
	@echo $(CUR_OBJS)

clean:
	rm $(OUT)/*.o
