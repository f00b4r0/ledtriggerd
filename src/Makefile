CFLAGS = -Wall
LDFLAGS = -luci -lubox
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)
MAIN := ledtriggerd

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
	$(RM) *.o *.d *~ $(MAIN)

-include $(DEPS)

.PHONY: all clean distclean install uninstall
