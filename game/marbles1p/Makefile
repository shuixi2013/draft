# Author: Li Zheng <lizheng_w5625@tp-link.net>


SUBDIRS		= $(shell ls -l | grep ^d | awk '{print $$9}')
CUR_SOURCE	= $(wildcard *.c)
CUR_OBJS	= $(patsubst %.c, %.o, $(CUR_SOURCE))


all: $(SUBDIRS) $(CUR_OBJS)

$(SUBDIRS): ECHO
	make -C $@

ECHO:
	@echo $(SUBDIRS)

$(CUR_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $(OUT)/$@ $(LDFLAGS) $(INCLUDE)

test:
	@echo $(CUR_OBJS)

clean:
	rm $(OUT)/*.o
