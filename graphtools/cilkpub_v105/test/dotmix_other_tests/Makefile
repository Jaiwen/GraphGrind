# Top-level folder containing include/cilkpub.
CILKPUB_TOP ?= ../..
CILKPUB_DIR=$(CILKPUB_TOP)/include
VERBOSE_LEVEL=2

# Test directory to place all the output tests.
TMP_FOLDER=tmp

include $(CILKPUB_TOP)/mk/platform_vars.mk

# Header files for the DotMix rgen tests.

HEADERS = dotmix_test_file_buffer.h dotmix_test_rgen_kernels.h

# Compile different driver programs based on the number of mixing
# iterations.
MIXING_ITER_LIST = 4 8 16
RGEN_DRIVER_FILES = $(addprefix dotmix_rgen_driver_, $(MIXING_ITER_LIST))

# All targets to build.
# The use of $(SLASH) is a hack to try to maintain
#  compatibility between icl and icc. icl may get confused if you pass
#  a file "tmp/foo.cpp"
TARGETS = $(addprefix $(TMP_FOLDER)$(SLASH), $(RGEN_DRIVER_FILES))

binaries: $(TARGETS)
	@echo "Target build list: " $(TARGETS)

$(TMP_FOLDER)$(SLASH)dotmix_rgen_driver_%: dotmix_rgen_driver.cpp $(HEADERS) $(TMP_FOLDER)
	$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) $< -o $@ -DDOTMIX_TEST_MIX_ITER=$*

# Hack for now.  Don't run some tests on native Windows.
ifeq ($(CILKPUB_OS), "WindowsNative")
run_tests: binaries rgen_run.py
	@echo "Not running some DotMix tests on native Windows."
	@echo "Test tests require python and this script"
	@echo "does not detect it..."

run_dieharder:
	@echo "Not running Dieharder on Windows for now."
else

run_tests: binaries rgen_run.py
	@echo "Running DotMix other tests"
	python rgen_run.py

run_dieharder: binaries rgen_run.py
	@echo "Running DieHarder tests to check RNG quality"
	@echo "WARNING: This test generates a file of random numbers that uses about 15 GB of disk space..."
	@echo "If you don't want to run this test, Ctrl-C to cancel..."
	@sleep 4
	@echo "Starting..."
	python rgen_run.py --run_dieharder
endif


.PHONY: clean run_tests run_dieharder

$(TMP_FOLDER):
	$(MKDIR_CMD) $(TMP_FOLDER)
clean: 
	$(RM_CMD) $(TMP_FOLDER) $(TMP_DEBUG) $(TARGETS)
