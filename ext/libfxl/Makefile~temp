LIB = libfxl

OBJECTS = \
        fxl6408.o \

DEPS += \
        libmsp \

override SRC_ROOT = ../../src

override CFLAGS += \
        -I$(SRC_ROOT)/include \
        -I$(SRC_ROOT)/include/$(LIB) \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)
