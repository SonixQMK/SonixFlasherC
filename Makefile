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
  CC = gcc
endif


#############  Mac
ifeq "$(OS)" "macos"

CFLAGS+=`pkg-config hidapi --cflags`
LIBS=-lhidapi -framework IOKit -framework CoreFoundation -framework AppKit
EXE=

endif

############# Windows
ifeq "$(OS)" "windows"

CFLAGS+=`pkg-config hidapi --cflags`
LIBS+= -lhidapi -lsetupapi -Wl,--enable-auto-import
EXE=.exe

endif

############ Linux (hidraw)
ifeq "$(OS)" "linux"

LIBS = `pkg-config libudev --libs`
CFLAGS+=`pkg-config hidapi-libusb --cflags`
LIBS+=`pkg-config hidapi-libusb --libs`
EXE=

endif


############# common

CFLAGS+=-Wall
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
	7z a sonixflasher-$(OS)-$(ARCH).zip sonixflasher$(EXE)
