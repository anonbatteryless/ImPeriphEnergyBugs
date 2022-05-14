LIB = liblsm


OBJECTS += \
  min.o \
  gyro.o \
  accl.o


DEPS += \
  libmspware\
  libmsp \
  libio\
  libmspbuiltins\
  libfxl


override SRC_ROOT = ../../src

override CFLAGS += \
  -I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)
