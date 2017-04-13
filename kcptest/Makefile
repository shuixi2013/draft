CFLAGS += -Wall -g -pthread

TARGET = isock
OBJS += ikcp.o isock.o main.o kcp_test.o

all: $(TARGET)

$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)

clean:
	-rm -f $(OBJS)
