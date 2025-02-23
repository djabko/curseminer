CC=gcc
CF_DEBUG=-Wall -g -DDEBUG
CF_OPTIM=-Ofast
CF_LIBS=-lncurses -lm -ldl -lpthread

SRCDIR=.
INCDIR=./include
OBJDIR=./obj
OPT_OBJDIR=./obj_opt

SRCS=$(wildcard $(SRCDIR)/*.c)
OBJS=$(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))
OPT_OBJS=$(patsubst $(SRCDIR)/%.c, $(OPT_OBJDIR)/%.o, $(SRCS))

TARGET=$(notdir $(shell pwd))
OPT_TARGET=$(TARGET)_opt


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CF_DEBUG) $(CF_LIBS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CF_DEBUG) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

optimize: $(OPT_TARGET)

$(OPT_TARGET): $(OPT_OBJS)
	$(CC) $(CF_OPTIM) $(CF_LIBS) -o $@ $^

$(OPT_OBJDIR)/%.o: $(SRCDIR)/%.c | $(OPT_OBJDIR)
	$(CC) $(CF_OPTIM) -I$(INCDIR) -c $< -o $@

$(OPT_OBJDIR):
	mkdir -p $(OPT_OBJDIR)

clean:
	rm -f $(TARGET) $(OPT_TARGET) $(OBJS) vgcore.* error.dump
	rm -rf $(OBJDIR) $(OPT_OBJDIR)
