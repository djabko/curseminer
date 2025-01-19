CC=gcc
CFLAGS=-Wall -g

SRCDIR=.
INCDIR=./include
OBJDIR=./obj

SRCS=$(wildcard $(SRCDIR)/*.c)
OBJS=$(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

TARGET=$(notdir $(shell pwd))


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -lncurses -lm -lglfw -lvulkan -ldl -lpthread -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -f $(TARGET) $(OBJS) vgcore.* error.dump
	rm -rf $(OBJDIR)
