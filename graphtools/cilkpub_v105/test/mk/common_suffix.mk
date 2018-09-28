#######################################################################
#  Copyright (C) 2012-13 Intel Corporation
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  *  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  *  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#  *  Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
#  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
#  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
#  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#######################################################################

# Top-level folder containing include/cilkpub.
CILKPUB_TOP ?= ..
VERBOSE_LEVEL=2

# Directory for include files for Cilkpub
CILKPUB_DIR=$(CILKPUB_TOP)/include

# Test directory to place all the output tests.
TMP_FOLDER=tmp

# Include platform-dependent definitions. 
include $(CILKPUB_TOP)/mk/platform_vars.mk


TARGETS = $(addprefix test_, $(TEST_NAMES)) 
RUN_TARGETS = $(addprefix run_test_, $(TEST_NAMES))
OTHER_TEST_TARGETS = $(addprefix run_other_, $(OTHER_TEST_DIRS))
CLEAN_OTHER_TARGETS = $(addprefix clean_other_, $(OTHER_TEST_DIRS))
RUN_PERF_TARGETS = $(addprefix run_perftest_, $(PERFTEST_NAMES))

# Tests for code under development.
DEV_TEST_TARGETS = $(addprefix test_, $(DEV_TEST_NAMES))
DEV_RUN_TARGETS = $(addprefix run_test_, $(DEV_TEST_NAMES))

# Temporary file where output data gets stored.
PERFLOG_PREFIX ?= cilkpub_perflog
PERF_RUNSCRIPT = ./cilkpub_perfrun.sh
PERF_MAXP ?= 8
PERF_REPS ?= 5

# Run all (correctness) test targets.
run_all: $(RUN_TARGETS) $(OTHER_TEST_TARGETS)

# Run all tests for modules under development.
dev_run: $(DEV_RUN_TARGETS)

# Build all tests for modules under development. 
dev_test: $(DEV_TEST_TARGETS)

# Run all performance test targets.
perf_all: $(RUN_PERF_TARGETS) 

binaries: $(TARGETS) 

# Build a generic test file.
test_%: test_%.cpp $(TMP_FOLDER)
	$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) $< -o $(TMP_FOLDER)/$@ $(LDFLAGS)

# Run a generic test file.
run_test_%: test_%
	$(TMP_FOLDER)/$< $(VERBOSE_LEVEL)

# Runs a test as a performance benchmark test.
run_perftest_%: test_%
	$(PERF_RUNSCRIPT) $(TMP_FOLDER)/$< $(PERF_MAXP) $(PERF_REPS) >> $(TMP_FOLDER)/$(PERFLOG_PREFIX)-$*.dat

run_other_%: %_other_tests
	cd $*_other_tests $(AND_SEP) make clean $(AND_SEP)  make run_tests

# Hack to recursively go into other_test directories and clean them.
clean_other_%: %_other_tests
	cd $*_other_tests $(AND_SEP) make clean


.PHONY: clean run_all perf_all

$(TMP_FOLDER):
	$(MKDIR_CMD) $(TMP_FOLDER)

clean: $(CLEAN_OTHER_TARGETS)
	$(RM_CMD) $(TMP_FOLDER) $(TMP_DEBUG)
