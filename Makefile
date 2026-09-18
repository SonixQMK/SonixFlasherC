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

ifeq (default,$(origin CC))
  CC = gcc
endif

BACKEND ?= libusb
STATIC ?= 0
PKG_CONFIG ?= pkg-config

ifeq ($(BACKEND),hidapi)
ifeq ($(OS),macos)
PKG_NAME = hidapi
else ifeq ($(OS),linux)
PKG_NAME = hidapi-libusb
else ifeq ($(OS),windows)
PKG_NAME = hidapi
else
$(error Unsupported OS for BACKEND=hidapi)
endif
else ifeq ($(BACKEND),libusb)
PKG_NAME = libusb-1.0
else
$(error Unsupported BACKEND='$(BACKEND)')
endif

define check_pkg_config
	@$(PKG_CONFIG) --exists $(PKG_NAME) || \
	  (echo "error: pkg-config package '$(PKG_NAME)' was not found" >&2; \
	   echo "       install the development package and try again" >&2; \
	   exit 1)
endef

ifeq "$(OS)" "macos"
ifeq "$(BACKEND)" "hidapi"
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_NAME))
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKG_NAME))
SRCS_BACKEND = src/usb_device_hidapi.c
else ifeq "$(BACKEND)" "libusb"
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_NAME))
ifeq "$(STATIC)" "1"
PKG_LIBS := $(shell $(PKG_CONFIG) --static --libs $(PKG_NAME))
else
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKG_NAME))
endif
SRCS_BACKEND = src/usb_device_libusb.c
endif
PKG_LIBS += -framework IOKit -framework CoreFoundation -framework AppKit
EXE=
endif

ifeq "$(OS)" "windows"
ifeq "$(BACKEND)" "hidapi"
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_NAME))
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKG_NAME))
SRCS_BACKEND = src/usb_device_hidapi.c
else ifeq "$(BACKEND)" "libusb"
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_NAME))
ifeq "$(STATIC)" "1"
PKG_LIBS := $(shell $(PKG_CONFIG) --static --libs $(PKG_NAME))
else
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKG_NAME))
endif
SRCS_BACKEND = src/usb_device_libusb.c
endif
PKG_LIBS += -lsetupapi -lwinmm -lole32 -static-libgcc
EXE=.exe
endif

ifeq "$(OS)" "linux"
ifeq "$(BACKEND)" "hidapi"
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_NAME))
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKG_NAME))
SRCS_BACKEND = src/usb_device_hidapi.c
else ifeq "$(BACKEND)" "libusb"
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_NAME))
ifeq "$(STATIC)" "1"
PKG_LIBS := $(shell $(PKG_CONFIG) --static --libs $(PKG_NAME))
else
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKG_NAME))
endif
SRCS_BACKEND = src/usb_device_libusb.c
endif
EXE=
endif

SRCS := $(filter-out src/usb_device_hidapi.c src/usb_device_libusb.c,$(wildcard src/*.c)) $(SRCS_BACKEND)
OBJS := $(SRCS:.c=.o)

CFLAGS += -Wall -Iinclude $(PKG_CFLAGS)
LIBS += $(PKG_LIBS)

CLANG ?= clang

all: sonixflasher
	@echo "Built with BACKEND=$(BACKEND) STATIC=$(STATIC)"

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

sonixflasher: $(OBJS)
	$(call check_pkg_config)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@$(EXE) $(LIBS)
ifeq "$(STATIC)" "1"
	strip $@$(EXE)
endif

.PHONY: release
release:
	$(MAKE) clean
	$(MAKE) STATIC=1 BACKEND=libusb all

clean:
	rm -f $(OBJS)
	rm -f sonixflasher$(EXE)

package: sonixflasher$(EXE)
	@echo "Packaging up sonixflasher for '$(OS)-$(ARCH)'"
	7z a sonixflasher-$(OS)-$(ARCH).zip sonixflasher$(EXE)

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