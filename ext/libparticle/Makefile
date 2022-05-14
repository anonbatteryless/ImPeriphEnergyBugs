LIB = libparticle


OBJECTS += \
  particle.o \


DEPS += \
  libmspware\
  libmsp \
  libio\
  libmspbuiltins


override SRC_ROOT = ../../src

override CFLAGS += \
  -I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)
