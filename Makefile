######################################################################################
##
## Makefile for Amiigo link
##
## Author: dashesy
##
##
## Major targets:
## all - make all the following targets:
##
## Special targets:
## install - place all scripts and binaries in the conventional directories
## clean - clean up
##
######################################################################################

#==========================================================================
# UTILITIES
#==========================================================================
MKPARENT := mkdir -p `dirname $(1)`
ECHO := @echo
CP := @cp


#==========================================================================
# CONSTANTS
#==========================================================================

CC := gcc
CXX := g++

OUTPUTBIN = amlink

# Additional libraries
LIBS := -lbluetooth

LFLAGS  = $(LIBDIRS) $(LIBS) 

INCLUDEDIRS := -I.

CFLAGS := -Wall $(INCLUDEDIRS)

# common sources
COMMON_SRC := ./main.c \
              ./btio.c \
              ./att.c \
              ./hcitool.c \

COMMON_OBJS := $(patsubst %.c, .obj/%.o, $(notdir $(COMMON_SRC)))

VPATH := $(sort  $(dir $(COMMON_SRC)))

all: prepare ./$(OUTPUTBIN)

install:
	@cp ./$(OUTPUTBIN) /usr/bin/$(OUTPUTBIN)

.PHONY: prepare
prepare:
	@mkdir -p .obj

.PHONY: clean
clean:
	rm -rf .obj
	rm  -f ./$(OUTPUTBIN)


# the "common" object files
.obj/%.o : %.cpp Makefile
	@echo creating $@ ...
	$(CXX) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
.obj/%.o : %.c Makefile
	@echo creating $@ ...
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
    
# This will make the output
./$(OUTPUTBIN): $(COMMON_OBJS)
	@echo building output ...
	$(CC) -o $(OUTPUTBIN) $(COMMON_OBJS) $(LFLAGS)
    
    