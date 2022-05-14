LIB = libcapybara

OBJECTS += \
  board.o \
	capybara.o \
	power.o \

ifneq ($(LIBCAPYBARA_SWITCH_DESIGN),)
OBJECTS += reconfig.o
endif # LIBCAPYBARA_SWITCH_DESIGN

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT)/include/$(LIB) \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)
