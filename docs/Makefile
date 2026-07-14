# This Makefile is used for TARFS development: copy doc/os_cygwin.c to this dir and 'make'
# 
.RECIPEPREFIX := >

# Compiler
CC      := gcc

# Directories
OBJDIR  := obj

# Flags
CFLAGS  := -Wall -Os -pedantic
CFLAGS  += -DTARSUM_BUILD=1 
CFLAGS += -MMD -MP 

# Общие исходники
COMMON_SRC := \
    file.c \
    tar.c \
    inode.c \
    os_cygwin.c \
    hash.c \
    fs.c \
    refc.c \
    posix.c \
    dir.c

# Для test
TEST_SRC := test.c $(COMMON_SRC)
TEST_OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(TEST_SRC))

# Для tarsum
TARSUM_SRC := tarsum.c $(COMMON_SRC)
TARSUM_OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(TARSUM_SRC))

# Object files
OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))

# Target executable

TARGETS := tarsum

.PHONY: all clean

all: $(TARGETS)

test: $(TEST_OBJ)
>$(CC) $^ -o $@

tarsum: $(TARSUM_OBJ)
>$(CC) $^ -o $@

$(OBJDIR)/%.o: %.c | $(OBJDIR)
>$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
>mkdir -p $(OBJDIR)

clean:
>rm -rf $(OBJDIR) $(TARGETS)

-include $(OBJ:.o=.d)
