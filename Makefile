TOOLS_REL_ROOT = tools
TOOLS = pacarana-pass
TOOLCHAINS = gcc clang pacarana-pass

# To build with defined test functions n'at, use the following command

# make bld/<toolchain>/all FUNC=<test_func> SETUP_FUNC=<setup_func> \
# CHECK_FUNC=<check_func> FUNC_PATH=<test_file> INPUTS=<inputs,to,test> \
# NUM_TRIALS=<#trials>

# Notes on command above
# <test_file> is the .c file with all of the test/setup/check functions in it.
# (note, we only drop in the part of the name *before* the .c because I'm lazy
# and didn't set up the filter myself)
# At the moment it also needs to be in the src/ directory, but that limitation
# isn't fundamental. maker is just being a pain about moving it anywhere else.

# check_func, setup_func and func are all just the function names
# inputs are macro'd in, so just separate them with commas, no additional parens

# <test_func> is the function under test, so it better be idempotent-- we're
# running it in a loop
# <setup_func> runs once at the very beginning
# <check_func> runs after the prescribed number of test runs (usually for
# outputting results as a sanity check)

#NUM_TRIALS is optional, default is 100 trials

export BOARD = capybara
export BOARD_MAJOR = 2
export BOARD_MINOR = 0
export DEVICE = msp430fr5994

EXEC = harness

# Set to 1 to run this code on a launchpad
export LIBCAPYBARA_FXL_OFF = 0
# Set to 1 to tell capy to hand off cold start-keep out to a JIT checkpointing
# mechanism
export LIBCAPYBARA_JIT = 1
export LIBPACARANA_JIT = 1

export USE_SENSORS ?= 1

ifeq ($(LIBCAPYBARA_JIT),1)
export CHKPT_TYPE = _chkpt
$(info chkpt type is: $(CHKPT_TYPE))
endif

#Edit setup func here, set up new USE_SENSORS to pull in mega kernels
#TODO this shouldn't be manual
ifneq ($(USE_SENSORS),)
export RUN_MEGA = 1
  #FUNC ?= align_mega
  #INPUTS ?= "b1,b2,b3,b4"
  #FUNC ?= dconv_mega
  #INPUTS ?= "b1,b2,b3,mat_c_dense_pseudo"
  #FUNC ?= dmm_mega
  #INPUTS ?= "b1,b2,b3,&mat_a_dense"
  #FUNC ?= dmv_mega
  #INPUTS ?= "b1,b2,b3,mat_a_dense_pseudo"
  #FUNC ?= dwt_mega
  #INPUTS ?= "b1,b2,b3"
  #FUNC ?= fft_mega
  #INPUTS ?= "b2,b3,b1,b4"
  FUNC ?= smv_mega
  INPUTS ?= "b1,b2,mat_a_sparse_pseudo"
  NUM_TRIALS ?= 9 #We choose 9 so we get 3 rounds with each size
  HPVLP ?= 1
  GYRO ?= 1
  RATE ?= 0x80
  SHORT_SAMPLE ?= 1
  SHORT_SAMPLE_LEN ?= 8
  OBJECTS = main_mega_kernels.o
override CFLAGS += -DUSE_SENSORS
# add in the pacarana files
override CFLAGS += -I../../tables/
else ifneq ($(RUN_PARTICLE),)
override CFLAGS += -I../../tables/
OBJECTS = main_particle.o
else ifnew ($(RUN_PARTICLE_DYN),)
override CFLAGS += -I../../tables/
OBJECTS = main_particle_filtered_dyn.o
else ifneq ($(RUN_MICRO),)
OBJECTS = main_micro_benchmark.o
else ifneq ($(RUN_TIMING),)
override CFLAGS += -I../../tables/
OBJECTS = main_timing_test.o
else ifneq ($(RUN_UNITS),)
override CFLAGS += -I../../tables/
ifeq ($(UNIT),)
OBJECTS = main_unit_test.o
else
OBJECTS = main_$(UNIT).o
endif
else ifneq ($(RUN_BUGS),)
override CFLAGS += -I../../tables/
OBJECTS = main_bugs.o
else
override CFLAGS += -I../../tables/
OBJECTS = main.o
endif # We picked what to run

# Settings for libpacarana timing measurements
export LIBPACARANA_MULTI_CNT = 1 # One counter per periph state
export LIBPACARANA_FULLY_DYNAMIC = 1 # No compiler assistance


DEPS =  libpudu:gcc libmspbuiltins:gcc libio:gcc librustic:gcc libcapybara:gcc  \
         libradio:gcc libmspuartlink:gcc libmsp:gcc liblsm:gcc libfxl:gcc libmspware:gcc\
        libapds:gcc  libparticle:gcc  \
        libalpaca:gcc libfixed:gcc libmat:gcc


# Set to 1 to use the librustic functions without logging
export LIBRUSTIC_UNSAFE = 1
# Se the toolchain that the *application* targeted by librustic will be compiled
# with
export LIBRUSTIC_TOOLCHAIN = gcc
# Set up timer grab to support libpacarana
export LIBRUSTIC_TIMERA_CAPTURE = 1

#TODO figure out how to derive this from the toolchain argument
ifneq ($(TOOLCHAIN),gcc)
export MULTI_TOOLCHAIN ?= 1
else
export MULTI_TOOLCHAIN ?=
endif
# Set to 1 to tell capy to hand off cold start-keep out to a JIT checkpointing
# mechanism

include Makefile.config

# We have to put all of the test options here instead of in another makefile
# because arguments don't see to get passed along

ifneq ($(NUM_TRIALS),)
override CFLAGS += -DNUM_TRIALS=$(NUM_TRIALS)
endif

ifneq ($(FUNC),)
ifeq ($(RUN_MEGA),)
override CFLAGS += -DFUNC=$(FUNC)
# Gross, brute force way of doing this
ifneq ($(filter align, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 0
export FUNC_NUM = 0
endif
ifneq ($(filter dconv, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 1
export FUNC_NUM = 1
endif
ifneq ($(filter dmm, $(FUNC)),)
$(info "in dmm!!!")
export LIBDSPKERNELS_FUNC_NUM = 2
export FUNC_NUM = 2
endif
ifneq ($(filter dmv, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 3
export FUNC_NUM = 3
endif
ifneq ($(filter dwt, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 4
export FUNC_NUM = 4
endif
ifneq ($(filter fft, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 5
export FUNC_NUM = 5
endif
ifneq ($(filter sconv, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 6
export FUNC_NUM = 6
endif
ifneq ($(filter smm, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 7
export FUNC_NUM = 7
endif
ifneq ($(filter smv, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 8
export FUNC_NUM = 8
endif
ifneq ($(filter sort, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 9
export FUNC_NUM = 9
endif
export LIBDSPKERNELS_FUNC_NUM
export FUNC_NUM
override CFLAGS += -DFUNC_NUM=$(LIBDSPKERNELS_FUNC_NUM)
else
override CFLAGS += -DFUNC=$(FUNC)
# Gross, brute force way of doing this
ifneq ($(filter align_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 0
export FUNC_NUM = 0
endif
ifneq ($(filter dconv_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 1
export FUNC_NUM = 1
endif
ifneq ($(filter dmm_mega, $(FUNC)),)
$(info "in dmm3!!!")
export LIBDSPKERNELS_FUNC_NUM = 2
export FUNC_NUM = 2
endif
ifneq ($(filter dmv_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 3
export FUNC_NUM = 3
endif
ifneq ($(filter dwt_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 4
export FUNC_NUM = 4
endif
ifneq ($(filter fft_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 5
export FUNC_NUM = 5
endif
ifneq ($(filter sconv_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 6
export FUNC_NUM = 6
endif
ifneq ($(filter smm_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 7
export FUNC_NUM = 7
endif
ifneq ($(filter smv_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 8
export FUNC_NUM = 8
endif
ifneq ($(filter sort_mega, $(FUNC)),)
export LIBDSPKERNELS_FUNC_NUM = 9
export FUNC_NUM = 9
endif
export LIBDSPKERNELS_FUNC_NUM
export FUNC_NUM
override CFLAGS += -DFUNC_NUM=$(LIBDSPKERNELS_FUNC_NUM)
endif
endif

#Quick defines
FUNC_PATH ?= $(FUNC)_funcs
SETUP_FUNC ?= $(FUNC)_init
CHECK_FUNC ?=
CLEANUP_FUNC ?= $(SETUP_FUNC)
DATA_FUNC ?= $(FUNC)_data
SHORT_DATA_FUNC ?= $(FUNC)_short_data


ifneq ($(FUNC_PATH),)
override CFLAGS += -DFUNC_PATH=$(FUNC_PATH)
ifneq ($(USE_SENSORS),)
override CFLAGS += -DFUNCS="\""$(FUNC_PATH)".c\""
else ifeq ($(RUN_PARTICLE),)
  ifeq ($(RUN_PARTICLE_DYN),)
  ifeq ($(RUN_SPACE),)
  ifeq ($(RUN_VIB),)
  ifeq ($(RUN_TIMING),)
  ifeq ($(RUN_UNITS),)
  ifeq ($(RUN_BUGS),)
override OBJECTS += $(FUNC_PATH).o
  endif
  endif
  endif
  endif
  endif
  endif
endif
endif

ifneq ($(SETUP_FUNC),)
override CFLAGS += -DSETUP_FUNC=$(SETUP_FUNC)
endif

ifneq ($(INPUTS),)
override CFLAGS += -DINPUTS_UT=$(INPUTS)
endif

ifneq ($(CHECK_FUNC),)
override CFLAGS += -DCHECK_FUNC=$(CHECK_FUNC)
endif

ifneq ($(CLEANUP_FUNC),)
override CFLAGS += -DCLEANUP_FUNC=$(CLEANUP_FUNC)
endif

ifneq ($(DATA_FUNC),)
override CFLAGS += -DDATA_FUNC=$(DATA_FUNC)
endif

# Short sampling
ifeq ($(SHORT_SAMPLE),1)
override CFLAGS += -DSHORT_SAMPLE
endif

ifneq ($(SHORT_SAMPLE_LEN),)
override CFLAGS += -DSHORT_SAMPLE_LEN=$(SHORT_SAMPLE_LEN)
endif

ifneq ($(SHORT_DATA_FUNC),)
override CFLAGS += -DSHORT_DATA_FUNC=$(SHORT_DATA_FUNC)
endif

#Stuff for sensors

ifeq ($(REENABLE),1)
override CFLAGS += -DREENABLE
endif


ifeq ($(HPVLP),1)
override CFLAGS += -DHPVLP
endif

ifeq ($(ACCEL),1)
override CFLAGS += -DACCEL
endif

ifeq ($(GYRO),1)
$(info gyro defined!)
override CFLAGS += -DGYRO
endif

ifeq ($(COLOR),1)
override CFLAGS += -DCOLOR
endif

ifeq ($(PROX),1)
override CFLAGS += -DPROX
endif

ifeq ($(TEMP),1)
override CFLAGS += -DTEMP
override CFLAGS += -DSENSE_FUNCS="\""temp_sensor.c"\""
endif

ifneq ($(RATE),)
override CFLAGS += -DRATE=$(RATE)
endif

ifneq ($(ITER_START),)
override CFLAGS += -DITER_START=$(ITER_START)
endif

# Test parameter


ifeq ($(SLEEP_OPT),1)
override CFLAGS += -DSLEEP_OPT
endif

export LIBDSPKERNELS_INPUT_SIZE ?= 3
export LIBDSPKERNELS_BLOCK_SIZE ?= 16

INPUT_SIZE ?=3
BLOCK_SIZE ?=16

override CFLAGS += -DBLOCK_SIZE=$(BLOCK_SIZE) \
                   -DINPUT_SIZE=$(INPUT_SIZE)

export CFLAGS

$(info Objects are: $(OBJECTS))


include tools/maker/Makefile
