# Directory for include files for Cilkpub
CILKPUB_DIR=../include
# Test directory to place all the output tests.
TMP_FOLDER=tmp

# Include platform-dependent definitions. 
include ../mk/platform_vars.mk

TARGETS = $(addprefix sample_, $(SAMPLE_NAMES)) $(addprefix csample, $(CSAMPLE_NAMES))
RUN_TARGETS = $(addprefix run_sample_, $(SAMPLE_NAMES)) $(addprefix run_csample_, $(CSAMPLE_NAMES))

run_all: $(RUN_TARGETS) 

binaries: $(TARGETS)

sample_%: sample_%.cpp $(TMP_FOLDER)
	$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) $< -o $(TMP_FOLDER)/$@ $(LDFLAGS)

run_sample_%: sample_%
	$(TMP_FOLDER)/$< 

csample_%: csample_%.c $(TMP_FOLDER)
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) $< -o $(TMP_FOLDER)/$@ $(LDFLAGS)

run_csample_%: csample_%
	$(TMP_FOLDER)/$< 

.PHONY: clean run_all


$(TMP_FOLDER):
	$(MKDIR_CMD) $(TMP_FOLDER)
clean: 
	$(RM_CMD) $(TMP_FOLDER) $(TMP_DEBUG)


