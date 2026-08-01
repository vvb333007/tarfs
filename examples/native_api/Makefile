# 
# 
.RECIPEPREFIX := >

# Compiler
CC      := gcc

# Directories
OBJDIR  := .

# Flags
CFLAGS  := -Wall -Wextra -Wno-sign-compare -Os -pedantic -I../../src
CFLAGS  += -DTARSUM_BUILD=1
CFLAGS += -MMD -MP 

# Common sources
COMMON_SRC := \
    ../../src/file.c \
    ../../src/tar.c \
    ../../src/inode.c \
    ../../tarsum/os_cygwin.c \
    ../../src/hash.c \
    ../../src/fs.c \
    ../../src/refc.c \
    ../../src/posix.c \
    ../../src/dir.c

TEST_SRC := native_api.c $(COMMON_SRC)
TEST_OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(TEST_SRC))


# Object files
OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))

# Target executable
TARGETS := native_api

.PHONY: all

all: $(TARGETS)  clean

native_api: $(TEST_OBJ)
>$(CC) $^ -o $@


$(OBJDIR)/%.o: %.c | $(OBJDIR)
>$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
>mkdir -p $(OBJDIR)

clean:
>rm -f *.o *.d ../../src/*.o ../../src/*.d ../../tarsum/*.o ../../tarsum/*.d

-include $(OBJ:.o=.d)




