# define our compiler
CC=gcc
# define our generic compiler flags
CFLAGS=-Wall -Wextra -std=c11
# define the paths to our third party libraries
LIBS=-lm -lpthread -latomic
# define the paths to our include directories
INCLUDES=-I./headers

# define variables for our source files
# we use find to grab them
SOURCES=$(shell find ./src -name '*.c')

# define folder paths and names
OBJ=obj
BIN=bin
TARGET=main

# setup up conditional build flags
# if debug is set to 1, add debug specific flags
ifeq ($(DEBUG), 1)
	CFLAGS += -DDEBUG=1 -ggdb
endif
# Release specific flags
ifeq ($(RELEASE), 1)
	CFLAGS += -O2
endif
# if SHARED flag is set, we prepare variables for building a shared/static library.
# we change the SOURCES variable to point to only the common source files.
# we also rename the TARGET to our library name.
ifeq ($(SHARED), 1)
    SOURCES=$(shell find ./src/lib -name '*.c')
    TARGET=my_lib
endif

# This variable is for our object files.
# We take the files in SOURCES and rename them to end in .o
# Then we add our OBJ folder prefix to all files.
OBJECTS=$(addprefix $(OBJ)/,$(SOURCES:%.c=%.o))

# We setup our default job
# it will build dependencies first then our source files.
.PHONY: all
all: src

# Build the source files.
# Conditional change to building for an executable or libraries.
# We also create the output BIN directory if it doesn't exist.
# This job depends on the OBJECT files.
.PHONY: src
src: $(OBJECTS)
	@mkdir -p $(BIN)
ifeq ($(SHARED), 1)
	$(CC) -shared -fPIC -o $(BIN)/$(TARGET).so $^ $(LIBS)
	ar -rcs $(BIN)/$(TARGET).a $^
else
	$(CC) $(CFLAGS) $^ -o $(BIN)/$(TARGET) $(LIBS)
endif

# Compile all source files to object files
# This job executes because the `src` job depends on all the files in OBJECTS
# which has the `$(OBJ)/%.o` file signature.
$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES)

# Job to clean out all object files and exe/libs.
.PHONY: clean
clean:
	@rm -rf $(OBJ)/* 2> /dev/null
	@rm -f $(BIN)/* 2> /dev/null

