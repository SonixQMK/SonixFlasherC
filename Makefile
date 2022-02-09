# path for HID
HIDAPI_DIR ?= ./hidapi

# try to do some autodetecting
UNAME := $(shell uname -s)
ARCH := $(shell uname -m)

ifeq "$(UNAME)" "Darwin"
	OS=macos
endif
ifeq "$(OS)" "Windows_NT"
	OS=windows
endif
ifeq "$(UNAME)" "Linux"
	OS=linux
endif

# deal with stupid Windows not having 'cc'
ifeq (default,$(origin CC))
  CC = gcc -Wall
endif


#############  Mac
ifeq "$(OS)" "macos"

CFLAGS+=-arch x86_64 -arch arm64
LIBS=-framework IOKit -framework CoreFoundation -framework AppKit
OBJS=$(HIDAPI_DIR)/mac/hid.o
EXE=

endif

############# Windows
ifeq "$(OS)" "windows"

LIBS += -lsetupapi -Wl,--enable-auto-import -static-libgcc -static-libstdc++
OBJS = $(HIDAPI_DIR)/windows/hid.o
EXE=.exe

endif

############ Linux (hidraw)
ifeq "$(OS)" "linux"

LIBS = `pkg-config libudev --libs`
OBJS = $(HIDAPI_DIR)/linux/hid.o
EXE=

endif


############# common

CFLAGS+=-I $(HIDAPI_DIR)/hidapi
OBJS += sonixflasher.o

all: sonixflasher

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


sonixflasher: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o sonixflasher$(EXE) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f sonixflasher$(EXE)

package: sonixflasher$(EXE)
	@echo "Packaging up sonixflasher for '$(OS)-$(ARCH)'"
	zip sonixflasher-$(OS)-$(ARCH).zip sonixflasher$(EXE)
