LIB = libfxl

DEVICE = msp430fr5949

OBJECTS = \
	fxl6408.o \

DEPS += \
	libmsp \
  libmspware \

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)
