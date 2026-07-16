# This Makefile is used for TARFS development: copy doc/os_cygwin.c to this dir and 'make'
# 
.RECIPEPREFIX := >

# Compiler
CC      := gcc

# Directories
OBJDIR  := .

# Flags
CFLAGS  := -Wall -Os -pedantic -I../src
CFLAGS  += -DTARSUM_BUILD=1
CFLAGS += -MMD -MP 

# Общие исходники
COMMON_SRC := \
    ../src/file.c \
    ../src/tar.c \
    ../src/inode.c \
    os_cygwin.c \
    ../src/hash.c \
    ../src/fs.c \
    ../src/refc.c \
    ../src/posix.c \
    ../src/dir.c

TARSUM_SRC := tarsum.c $(COMMON_SRC)
TARSUM_OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(TARSUM_SRC))

# Object files
OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))

# Target executable
TARGETS := tarsum

.PHONY: all

all: $(TARGETS)  clean

tarsum: $(TARSUM_OBJ)
>$(CC) $^ -o $@

$(OBJDIR)/%.o: %.c | $(OBJDIR)
>$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
>mkdir -p $(OBJDIR)

clean:
>rm -f *.o *.d ../src/*.o ../src/*.d

-include $(OBJ:.o=.d)
