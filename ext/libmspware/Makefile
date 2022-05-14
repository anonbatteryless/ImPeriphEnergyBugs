LIB = libmspware

# FAMILY is defined by Maker
# This switch is not exhaustive, we has branches only for families we use so far
ifeq ($(FAMILY),MSP430FR)

OBJECTS = \
	adc12_b.o \
	aes256.o \
	comp_e.o \
	crc32.o \
	crc.o \
	cs.o \
	dma.o \
	esi.o \
	eusci_a_spi.o \
	eusci_a_uart.o \
	eusci_b_i2c.o \
	eusci_b_spi.o \
	framctl.o \
	gpio.o \
	lcd_c.o \
	mpu.o \
	mpy32.o \
	pmm.o \
	ram.o \
	ref_a.o \
	rtc_b.o \
	rtc_c.o \
	sfr.o \
	sysctl.o \
	timer_a.o \
	timer_b.o \
	tlv.o \
	wdt_a.o \
	framctl_a.o \
	hspll.o \
	mtif.o \
	saph.o \
	sdhs.o \
	uups.o \

else ifeq ($(FAMILY),MSP430F)

OBJECTS = \
	adc10_a.o \
	ctsd16.o \
	eusci_b_spi.o \
	mpy32.o \
	rtc_a.o \
	tec.o \
	usci_a_spi.o \
	adc12_a.o \
	dac12_a.o \
	flashctl.o \
	oa.o \
	rtc_b.o \
	timer_a.o \
	usci_a_uart.o \
	aes.o \
	dma.o \
	gpio.o \
	pmap.o \
	rtc_c.o \
	timer_b.o \
	usci_b_i2c.o \
	battbak.o \
	eusci_a_spi.o \
	lcd_b.o \
	pmm.o \
	sd24_b.o \
	timer_d.o \
	usci_b_spi.o \
	comp_b.o \
	eusci_a_uart.o \
	lcd_c.o \
	ram.o \
	sfr.o \
	tlv.o \
	wdt_a.o \
	crc.o \
	eusci_b_i2c.o \
	ldopwr.o \
	ref.o \
	sysctl.o \
	ucs.o \

else # FAMILY
$(error Unsupported or unimplemented MCU family)
endif # FAMILY

include ../../Makefile.family # defines FAMILY_DIR

override SRC_ROOT = ../../src/$(FAMILY_DIR)

override CFLAGS += \
	-I$(SRC_ROOT) \
	-Wno-parentheses -Wno-maybe-uninitialized \

include $(MAKER_ROOT)/Makefile.$(TOOLCHAIN)
