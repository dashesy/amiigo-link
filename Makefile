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
COMMON_SRC := ./main.cpp \
              ./btio.c \
              ./att.c \

VPATH := $(sort  $(dir $(COMMON_SRC)))

COMMON_OBJS := $(patsubst %.cpp, .obj/%.o, $(notdir $(COMMON_SRC)))

# the "common" object files
.obj/%.o : %.cpp Makefile
	@echo creating $@ ...
	$(CXX) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
.obj/%.o : %.c Makefile
	@echo creating $@ ...
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
    
# This will make the output
$(BinDir)/$(OUTPUTBIN): $(COMMON_OBJS)
	@echo building output ...
	$(CC) -o $(OUTPUTBIN) $(COMMON_OBJS) $(LFLAGS)
    
    