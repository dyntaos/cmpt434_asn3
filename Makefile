###################################################
##            CMPT 434 - Assignment 3            ##
##          University of Saskatchewan           ##
##                     2020                      ##
##-----------------------------------------------##
##                  Kale Yuzik                   ##
##                kay851@usask.ca                ##
##      NSID: kay851     Student #11071571       ##
###################################################


CC = gcc
CFLAGS =
CPPFLAGS = -Wall -Wextra -pedantic -g
LDFLAGS =

ARCH = $(shell uname -s)$(shell uname -m)

BUILD = ./build
BIN = $(BUILD)/bin/$(ARCH)
OBJ = $(BUILD)/obj/$(ARCH)
LIB = $(BUILD)/lib/$(ARCH)


.PHONY: all mkdirs clean

all: mkdirs $(BIN)/router

mkdirs:
	mkdir -p $(BIN) $(OBJ) $(LIB)

clean:
	rm -rf ./build ./router


$(OBJ)/tcp.o: tcp.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJ)/router.o: router.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BIN)/router: $(OBJ)/router.o $(OBJ)/tcp.o
	$(CC) -o $@ $^
	ln -fs $@ ./router
