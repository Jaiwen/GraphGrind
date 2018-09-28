#######################################################################
#  Copyright (C) 2013 Intel Corporation
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


# Common object definitions.
PERF_COMMON := PerfDataPoint.py PerfDataSet.py PerfDataParser.py

# Independent scripts.
PERF_SCRIPTS := perf_diff_files.py perf_speedup_plot.py perf_summarize_file.py

# Name of output directory. (hardcoded name from perf_speedup_plot.py). 
GPLOT_OUTPUT_DIR := gplot_output

# Sample input files.
SAMPLE_DATA_DIR := sample_data
SAMPLE_DATA1 := $(SAMPLE_DATA_DIR)/qsort.dat
SAMPLE_DATA2 := $(SAMPLE_DATA_DIR)/ssort.dat

OUTPUT_DIFF1   := sample_diff1.txt
OUTPUT_DIFF2   := sample_diff2.txt
OUTPUT_SUMMARY := summary_diff.tmp

# Output files from this Makefile.
ALL_OUTPUTS := $(OUTPUT_DIFF1) $(OUTPUT_DIFF2) $(GPLOT_OUTPUT_DIR) *.png

all: sample_speedup sample_diff

# Generate sample diff.  Compares min and average of $(SAMPLE_DATA2)
sample_diff: $(SAMPLE_DATA_FILE) $(PERF_COMMON) $(PERF_SCRIPTS)
	python perf_summarize_file.py $(SAMPLE_DATA2) $(OUTPUT_SUMMARY) min                      # Summary data file 1 via avg
	python perf_summarize_file.py $(SAMPLE_DATA2) $(OUTPUT_SUMMARY) avg                      # Summarize data file 1 for avg.
	python perf_diff_files.py min_$(OUTPUT_SUMMARY) avg_$(OUTPUT_SUMMARY) $(OUTPUT_DIFF2)  # Create output txt files.
	echo "Diff between min and average for $(SAMPLE_DATA2)"
	cat $(OUTPUT_DIFF2)

# Generate a sample speedup plot comparing $(SAMPLE_DATA1) and $(SAMPLE_DATA2)
sample_speedup: $(SAMPLE_DATA1) $(SAMPLE_DATA2) $(PERF_COMMON) $(PERF_SCRIPTS)
	python perf_speedup_plot.py $(SAMPLE_DATA1) $(SAMPLE_DATA2)                # Speedup plots comparing  data files 1 and 2.
	cd $(GPLOT_OUTPUT_DIR); gnuplot *.plt                                      # Run gnuplot
	mv $(GPLOT_OUTPUT_DIR)/*.png .                                             # Move images to current directory.
	python perf_diff_files.py $(SAMPLE_DATA1) $(SAMPLE_DATA2) $(OUTPUT_DIFF1)  # Create output txt files.
	echo "Diff between $(SAMPLE_DATA1) and $(SAMPLE_DATA2)"
	cat $(OUTPUT_DIFF1)  # Display output difference.

.PHONY: all clean sample_diff sample_speedup 

clean:
	rm -rf *.pyc *.tmp 

extra_clean:
	rm -rf *.pyc *.tmp $(ALL_OUTPUTS)
