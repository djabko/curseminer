CC=gcc
CF_DEBUG=-Wall -g -DDEBUG
CF_OPTIM=-Ofast
CF_LIBS=-lncurses -lm -ldl -lpthread -lSDL2
SDL2_CFLAGS := $(shell sdl2-config --cflags)
SDL2_LIBS := $(shell sdl2-config --libs)

SRCDIR=./src
INCDIR=./include
OBJDIR=./obj
LOGSDIR=./logs
OPT_OBJDIR=./obj_opt

SRCS=$(wildcard $(SRCDIR)/*.c) $(wildcard ./src/games/*.c)
OBJS = $(patsubst ./src/%.c, $(OBJDIR)/%.o, $(wildcard ./src/*.c)) \
       $(patsubst ./src/games/%.c, $(OBJDIR)/games/%.o, $(wildcard ./src/games/*.c))
OPT_OBJS=$(patsubst $(SRCDIR)/%.c, $(OPT_OBJDIR)/%.o, $(SRCS))

TARGET=$(notdir $(shell pwd))
OPT_TARGET=$(TARGET)_opt


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CF_DEBUG) $(SDL2_CFLAGS) $(CF_LIBS) -o $@ $^

$(OBJDIR)/%.o: ./src/%.c | $(OBJDIR)
	$(CC) $(CF_DEBUG) $(SDL2_CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/games/%.o: ./src/games/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CF_DEBUG) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

optimize: $(OPT_TARGET)

$(OPT_TARGET): $(OPT_OBJS)
	$(CC) $(CF_OPTIM) $(SDL2_CFLAGS) $(CF_LIBS) -o $@ $^

$(OPT_OBJDIR)/%.o: $(SRCDIR)/%.c | $(OPT_OBJDIR)
	$(CC) $(CF_OPTIM) $(SDL2_CFLAGS) -I$(INCDIR) -c $< -o $@

$(OPT_OBJDIR):
	mkdir -p $(OPT_OBJDIR)

clean:
	rm -f $(TARGET) $(OPT_TARGET) $(OBJS) $(LOGSDIR)/error.dump vgcore.* error.dump gmon.out
	rm -rf $(OBJDIR) $(OPT_OBJDIR)
