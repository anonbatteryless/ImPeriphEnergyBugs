LIB = librustic

OBJECTS += \
  runtime.o \

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT)/include/$(LIB) \

ifeq ($(LIBRUSTIC_UNSAFE),1)
LOCAL_CFLAGS += -DLIBRUSTIC_UNSAFE
endif

ifeq ($(LIBRUSTIC_CLANG_ISR),1)
LOCAL_CFLAGS += -DLIBRUSTIC_CLANGISR
endif

ifeq ($(LIBRUSTIC_TOOLCHAIN),gcc)
LOCAL_CFLAGS += -DLIBRUSTIC_USEMSP430X
endif

ifeq ($(LIBRUSTIC_TIMERA_CAPTURE),1)
LOCAL_CFLAGS += -DLIBRUSTIC_TIMERA
endif

override CFLAGS += $(LOCAL_CFLAGS)

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)


