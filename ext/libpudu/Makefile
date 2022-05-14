LIB = libpacarana

OBJECTS = paca_init.o \
          pacarana.o

DEPS += librustic libalpaca libmsp libfxl 

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT)/include/$(LIB)/ \

ifneq ($(LIBPACARANA_JIT),)
LOCAL_CFLAGS +=  -DLIBPACARANA_JIT
endif

ifneq ($(LIBPACARANA_MULTI_CNT),)
LOCAL_CFLAGS += -DLIBPACARANA_MULTI_CNT
endif

ifneq ($(LIBPACARANA_FULLY_DYNAMIC),)
LOCAL_CFLAGS += -DLIBPACARANA_FULLY_DYNAMIC
endif

override CFLAGS += $(LOCAL_CFLAGS)

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)


