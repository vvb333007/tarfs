.RECIPEPREFIX := >

# Compiler
CC      := gcc

# Directories
OBJDIR  := obj

# Flags
CFLAGS  := -Wall -std=c11 -D_POSIX_C_SOURCE=200809L -g
CFLAGS += -MMD -MP

# Sources
SRC := \
    test.c \
    tar.c \
    inode.c \
    os_cygwin.c \
    fnv1a.c \
    fs.c \
    refc.c

# Object files
OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))

# Target executable
TARGET := test

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
>$(CC) $(OBJ) -o $@

$(OBJDIR)/%.o: %.c | $(OBJDIR)
>$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
>mkdir -p $(OBJDIR)

clean:
>rm -rf $(OBJDIR) $(TARGET)

-include $(OBJ:.o=.d)
