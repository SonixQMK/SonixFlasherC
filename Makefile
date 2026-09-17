# try to do some autodetecting
UNAME := $(shell uname -s)
ARCH := $(shell uname -m)

ifeq ($(UNAME),Darwin)
	OS := macos
else ifeq ($(UNAME),Linux)
	OS := linux
else ifneq ($(findstring MINGW,$(UNAME)),)
	OS := windows
else ifneq ($(findstring MSYS,$(UNAME)),)
	OS := windows
else ifeq ($(OS),Windows_NT)
	OS := windows
endif

# deal with stupid Windows not having 'cc'
ifeq (default,$(origin CC))
  CC = gcc
endif

# Select USB backend: libusb (default) or hidapi
BACKEND ?= libusb

#############  Mac
ifeq "$(OS)" "macos"

ifeq "$(BACKEND)" "hidapi"
CFLAGS+=`pkg-config hidapi --cflags`
LIBS=-lhidapi -framework IOKit -framework CoreFoundation -framework AppKit
SRCS_BACKEND=src/usb_device_hidapi.c
else ifeq "$(BACKEND)" "libusb"
CFLAGS+=`pkg-config libusb-1.0 --cflags`
LIBS=`pkg-config libusb-1.0 --libs` -framework IOKit -framework CoreFoundation -framework AppKit
SRCS_BACKEND=src/usb_device_libusb.c
endif
EXE=

endif

############# Windows
ifeq "$(OS)" "windows"

ifeq "$(BACKEND)" "hidapi"
CFLAGS+=`pkg-config hidapi --cflags`
LIBS+= -lhidapi -lsetupapi -Wl,--enable-auto-import
SRCS_BACKEND=src/usb_device_hidapi.c
else ifeq "$(BACKEND)" "libusb"
CFLAGS+=`pkg-config libusb-1.0 --cflags`
LIBS+=`pkg-config libusb-1.0 --libs` -Wl,--enable-auto-import
SRCS_BACKEND=src/usb_device_libusb.c
endif
EXE=.exe

endif

############ Linux (hidraw)
ifeq "$(OS)" "linux"

ifeq "$(BACKEND)" "hidapi"
LIBS = `pkg-config libudev --libs`
CFLAGS+=`pkg-config hidapi-libusb --cflags`
LIBS+=`pkg-config hidapi-libusb --libs`
SRCS_BACKEND=src/usb_device_hidapi.c
else ifeq "$(BACKEND)" "libusb"
CFLAGS+=`pkg-config libusb-1.0 --cflags`
LIBS+=`pkg-config libusb-1.0 --libs`
SRCS_BACKEND=src/usb_device_libusb.c
endif
EXE=

endif


############# common

SRCS := $(filter-out src/usb_device_hidapi.c src/usb_device_libusb.c,$(wildcard src/*.c)) $(SRCS_BACKEND)
OBJS := $(SRCS:.c=.o)

CFLAGS += -Wall -Iinclude
CLANG ?= clang

all: sonixflasher
	@echo "Built with BACKEND=$(BACKEND)"

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

sonixflasher: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@$(EXE) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f sonixflasher$(EXE)

package: sonixflasher$(EXE)
	@echo "Packaging up sonixflasher for '$(OS)-$(ARCH)'"
	7z a sonixflasher-$(OS)-$(ARCH).zip sonixflasher$(EXE)

############# lint

WARNINGS = \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Wmissing-prototypes \
	-Wstrict-prototypes \
	-Wmissing-declarations \
	-Wold-style-definition \
	-Wshadow \
	-Wformat=2 \
	-Wundef \
	-Wvla \
	-Wwrite-strings \
	-Wcast-qual \
	-Wpointer-arith \
	-Wconversion \
	-Wsign-conversion \
	-Wdouble-promotion \
	-Wnull-dereference

.PHONY: lint-compile
lint-compile:
	@echo "Syntax-checking with $(CLANG) (BACKEND=$(BACKEND))..."
	@for src in $(SRCS); do \
		echo "  $$src"; \
		$(CLANG) $(CFLAGS) $(WARNINGS) -fsyntax-only $$src || exit 1; \
	done