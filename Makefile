# Library variables
LIBNAME := libjsonXstruct
LIBVERSION := 0.0.1
VER_MAJOR := $(shell echo "$(LIBVERSION)" | sed -rn 's/([0-9]+)\.[0-9]+\.[0-9]+/\1/p')
VER_MINOR := $(shell echo "$(LIBVERSION)" | sed -rn 's/[0-9]+\.([0-9]+)\.[0-9]+/\1/p')
VER_PATCH := $(shell echo "$(LIBVERSION)" | sed -rn 's/[0-9]+\.[0-9]+\.([0-9]+)/\1/p')
LIB_OBJ := $(CURDIR)/jsonXstruct.o
DEBUG := 1
# Compiler variables
AR 		:= $(CROSS_PREFIX)ar
RANLIB	:= $(CROSS_PREFIX)ranlib
AS 		:= $(CROSS_PREFIX)as
CC		:= $(CROSS_PREFIX)gcc -std=c99
CXX		:= $(CROSS_PREFIX)g++
CPP		:= $(CC) -E
ARFLAGS := crv
ASFLAGS :=
CFLAGS	:=	-Wall -Wextra -Werror -W -fPIC -Wstrict-prototypes \
			-Wwrite-strings -Wshadow -Winit-self -Wcast-align -Wformat=2 \
			-Wmissing-prototypes -Wstrict-overflow=2 -Wcast-qual -Wc++-compat \
			-Wundef -Wswitch-default -Wconversion -D_GNU_SOURCE
CXXFLAGS:=
CPPFLAGS:=	-I$(CURDIR) -I./deps/include/json-c
LDFLAGS	:=
LDLIBS	:=
export
ifeq ($(DEBUG),1)
    CFLAGS += -g
else
    CFLAGS += -Os
endif

# validate gcc version for use fstack-protector-strong
MIN_GCC_VERSION = "4.9"
GCC_VERSION := "`$(CC) -dumpversion`"
IS_GCC_ABOVE_MIN_VERSION := $(shell expr "$(GCC_VERSION)" ">=" "$(MIN_GCC_VERSION)")
ifeq ($(IS_GCC_ABOVE_MIN_VERSION),1)
    CFLAGS += -fstack-protector-strong
else
    CFLAGS += -fstack-protector
endif

.PHONY: clean all shared static tests
all: shared static tests
shared: $(LIBNAME).so
static: $(LIBNAME).a
tests:
	-$(MAKE) -C $(CURDIR)/example
clean:
	-$(RM) $(LIB_OBJ)
	-$(RM) $(LIBNAME).so*
	-$(RM) $(LIBNAME).a
	-$(MAKE) -C $(CURDIR)/example clean

# static libraries
$(LIBNAME).a: $(LIB_OBJ)
	@echo "Build static library '$@'..."
	$(AR) $(ARFLAGS) $@ $^
	-@($(RANLIB) $@ || true) >/dev/null 2>&1

# shared libraries
$(LIBNAME).so: $(LIBNAME).so.$(VER_MAJOR)
	ln -s $< $@
$(LIBNAME).so.$(VER_MAJOR): $(LIBNAME).so.$(LIBVERSION)
	ln -s $< $@
$(LIBNAME).so.$(LIBVERSION): LDFLAGS += -Wl,-soname=$(LIBNAME).so.$(VER_MAJOR)
$(LIBNAME).so.$(LIBVERSION): $(LIB_OBJ)
	@echo "Build shared library '$@'..."
	$(CC) -shared $(LDFLAGS) $^ -o $@

# Pattern Rule
%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@
%.o: %.cxx
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@
%.o: %.c++
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@
