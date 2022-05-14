CXXFLAGS = -rdynamic $(shell llvm-config-7 --cxxflags) -g -O0 -fexceptions

cxxflags-cheat = -I/usr/include\
-march=x86-64\
-mtune=generic\
-O2\
-pipe\
-fstack-protector-strong\
-fno-plt\
-fPIC\
-fvisibility-inlines-hidden\
-Werror=date-time\
-std=c++11\
-Wall\
-Wextra\
-Wno-unused-parameter\
-Wwrite-strings\
-Wcast-qual\
-Wno-missing-field-initializers\
-pedantic\
-Wno-long-long\
-Wno-maybe-uninitialized\
-Wno-class-memaccess\
-Wdelete-non-virtual-dtor\
-Wno-comment\
-fdiagnostics-color\
-ffunction-sections\
-fdata-sections\
-O3\
-DNDEBUG\
-D_GNU_SOURCE\
-D__STDC_CONSTANT_MACROS\
-D__STDC_FORMAT_MACROS\
-D__STDC_LIMIT_MACROS

CXXFLAGS += $(cxxflags-cheat)

TEST_ALL=0

ifeq ($(TEST_ALL),1)
CXXFLAGS += -DTEST_SPEC\
            -DTEST_ISR\
            -DTEST_MOD \
            -DTEST_AUTO \
						-DTEST_EXTRA

endif

# TEST_INF is deprecated, it was used to test the inference functionality where
# we guessed how many cycles a function would take by counting the number of
# lines of llvm ir
ifeq ($(TEST_INF),1)
CXXFLAGS += -DTEST_INF
endif

# TEST_ISR enables testing the effects of interrupts. If this is enabled we'll
# add the effect of the ISRs to every allowed point in the program. We demarcate
# allowed regions using the isr enable/disable functions which this option will
# also identify.
ifeq ($(TEST_ISR),1)
CXXFLAGS += -DTEST_ISR
endif

# TEST_SPEC compares the user's specification for the allowed typestates at each
# program point to the possible reaching typestates
ifeq ($(TEST_SPEC),1)
CXXFLAGS += -DTEST_SPEC
endif

# TEST_VERBOSE prints out extra information for testing, it doesn't change the
# functionality of the pass
ifeq ($(TEST_VERBOSE),1)
CXXFLAGS += -DTEST_VERBOSE
endif

# TEST_TG prints out extra information about the task graph that we build up
ifeq ($(TEST_TG),1)
CXXFLAGS += -DTEST_TG
endif

# TEST_TF runs a unit test on the transfer function
ifeq ($(TEST_TF),1)
CXXFLAGS += -DTEST_TF
endif

# TEST_ATTR prints out extra information about the attributes we add to
# functions that are labeled drivers and the peripheral variables we've labeled
ifeq ($(TEST_ATTR),1)
CXXFLAGS += -DTEST_ATTR
endif

# TEST_DRVR prints out extra information about the driver functions, it doesn't
# change any pass functionality
ifeq ($(TEST_DRVR),1)
CXXFLAGS += -DTEST_DRVR
endif

# TEST_MEET prints extra information about the meet operation
ifeq ($(TEST_MEET),1)
CXXFLAGS += -DTEST_MEET
endif

# TEST_WL prints extra information about the worklist that we build up
ifeq ($(TEST_WL),1)
CXXFLAGS += -DTEST_WL
endif


# TEST_EXTRA reports the possible typestates for each point in each function
# _from each call site_ so you could use the information to figure out the worst
# case power consumption during a function call
ifeq ($(TEST_EXTRA),1)
CXXFLAGS += -DTEST_EXTRA
endif

# TEST_MOD breaks apart basic blocks so that we can use the broken blocks as
# points to add in the effect of the ISR (in other words, we need to run
# TEST_MOD and TEST_ISR together)
ifeq ($(TEST_MOD),1)
CXXFLAGS += -DTEST_MOD
endif

# TEST_AUTO adds disable/enable calls to the application based on function
# profiling and peripheral profiling.
ifeq ($(TEST_AUTO),1)
CXXFLAGS += -DTEST_AUTO
endif

ifeq ($(TEST_ATOMIC),1)
CXXFLAGS += -DTEST_ATOMIC
endif

SRC = ./src
BIN = ./bld
CXXFLAGS += -I $(SRC)
VPATH = $(SRC)
SOURCES = $(wildcard $(SRC)/*.cpp)
OBJS = $(patsubst $(SRC)/%.cpp,%.o,$(SOURCES))

tasks: tasks.so

all: tasks.so

dataflow.o: dataflow.cpp dataflow.h

path_check.o: path_check.cpp path_check.h

int_concurrency.o: int_concurrency.cpp int_concurrency.h tasks.cpp

extra_function_check.o: extra_function_check.cpp extra_function_check.h

inference.o: inference.cpp inference.h

auto_activation.o: auto_activation.cpp auto_activation.h

atomic_report.o: atomic_report.cpp atomic_report.h

#tasks.so: tasks.o dataflow.o path_check.o int_concurrency.o \
#extra_function_check.o inference.o auto_activation.o

tasks.so: $(OBJS)

	g++ -dylib -shared $^ -o $@

clean:
	rm -f *.o *~ *.so *.out *.bc *.ll

.PHONY: clean all

