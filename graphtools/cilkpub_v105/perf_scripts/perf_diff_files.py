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

##
# \file perf_diff_files.py
#
# \brief Reads in two data files, and generates a summary "diff"
#  output file comparing the two runs.
#
# \author  Jim Sukha
# \version 1.02

import csv
import os
import string 
import sys

from PerfDataPoint import *
import PerfDataParser


## Threshold for percent difference that we should report.
CILKPUB_DIFF_REPORT_THRESHOLD = 1.0

## Threshold for percent difference that we should flag as large.
CILKPUB_DIFF_LARGE_THRESHOLD = 5.0

## Run grep on specified file, keeping any line containing one of the
## target strings.
#
# @param fname           Name of input file.
# @param outfile         Name of output file to create.
# @param target_strings  List of target strings.
#
def run_grep_filter(fname, outfile, target_strings):
    grep_cmd = "grep "
    for s in target_strings:
        grep_cmd = grep_cmd + " -e \"%s\" " % s

    grep_cmd = grep_cmd +  " %s > %s" % (fname, outfile)
    print "#  Grep command: %s" % grep_cmd
    os.system(grep_cmd)

## Generates an output file summarizing the performance difference
# between two specified data sets.
#
# @param base_data          Baseline data set.
# @param new_data           New data set to compare against baseline.
# @param outfile_name       Name of output summary file to create.
# @param large_diff_thresh  Percent difference to flag as large.
# @param any_diff_thresh    Percent difference to report.
#    
def generate_performance_diff(base_data,
                              new_data,
                              outfile_name,
                              large_diff_thresh=CILKPUB_DIFF_LARGE_THRESHOLD,
                              any_diff_thresh=CILKPUB_DIFF_REPORT_THRESHOLD):

    out = open(outfile_name, "w")

    header_string = "# %20s %15s %15s %8s" %  ("Benchmark ",
                                               base_data.get_desc(),
                                               new_data.get_desc(),
                                               "Percent Diff")

    new_data_keys = sorted(new_data.point_map.keys())

    # Record which tests whose diff varies by more than a certain percent.
    slower_diff_tests = [header_string]
    faster_diff_tests = [header_string]
    any_diff_tests = [header_string]
    
    # Also generate a full report.
    full_log = [header_string]
    
    for key in new_data_keys:
        n = new_data.point_map[key]
        if (key in base_data.point_map):
            b = base_data.point_map[key]

            # Look at an average time, throwing away the min and max
            # times.
            btime = b.get_avg_time_without_minmax()
            ntime = n.get_avg_time_without_minmax()

            if (btime > 0) and (ntime > 0):
                percent_diff = (ntime - btime) * 100.0 / (btime)
            else:
                print "# WARNING: suspicious data point with btime = %f, ntime = %f" % (btime, ntime)
                percent_diff = 1e300
                
            summary = "%40s, %8g, %8g, %8g" % (b.key_string(),
                                               btime,
                                               ntime,
                                               percent_diff)
            full_log.append(summary)

            if (percent_diff >= large_diff_thresh):
                slower_diff_tests.append(summary)
            elif (percent_diff <= -large_diff_thresh):
                faster_diff_tests.append(summary)
            if (abs(percent_diff) >= any_diff_thresh):
                any_diff_tests.append(summary)
        else:
            print "# NEW DATA POINT: %s" % n.key_string()

    out.write("#               [Test with change >= %f percent]\n" % large_diff_thresh)
    out.write("\n")
    for line in slower_diff_tests:
        out.write(line + "\n")
    out.write("\n\n")

    out.write("#               [Test with change <= -%f percent]\n" % large_diff_thresh)
    out.write("\n")
    for line in faster_diff_tests:
        out.write(line + "\n")
    out.write("\n\n")

    out.write("# Any diff >= %f percent \n" % any_diff_thresh)
    out.write("\n")
    for line in any_diff_tests:
        out.write(line + "\n")
    out.write("\n\n")

    out.write("# All data \n")
    for line in full_log:
        out.write(line + "\n")
    out.write("\n")
        
    out.close()


## Gets the PerfDataSet object from a file for a given target string set.
def get_perf_data(fname_base, tmp_file_name, target_string_list):
    base_data = None
    if (fname_base):
        for target_strings in target_string_list:
            run_grep_filter(fname_base, tmp_file_name, target_strings)
            if base_data:
                tmp_data = PerfDataParser.parse_data_file(tmp_file_name,
                                                          default_desc=fname_base)
                base_data.merge(tmp_data)
            else:
                base_data = PerfDataParser.parse_data_file(tmp_file_name,
                                                           default_desc=fname_base)
            os.system("rm %s" % tmp_file_name)
    return base_data


## Script to generate performance difference data between two input
# files.
#
# Usage: <this script> <input_file1> [input_file2] [output_file]
#
def main():
    fname_base = None
    fname_new = None
    base_data = None
    new_data = None

    perf_data_base_tmp = "perf_data_base.tmp"
    perf_data_new_tmp = "perf_data_new.tmp"
    perf_data_cilktest_base_tmp = "perf_data_cilktest_base.tmp"
    perf_data_cilktest_new_tmp = "perf_data_cilktest_new.tmp"
    perf_diff_outfile = "perf_diff_output.txt"

    if (len(sys.argv) > 1):
        fname_base = sys.argv[1]
        print "#  Base file: %s " % fname_base
    else:
        print "This script generates a performance difference data between up"
        print "to two input files."
        print "Usage: %s <input_file1> [input_file2] [output_file]" % sys.argv[0]
        exit(0)

    # If we have two files, compute a diff.
    if (len(sys.argv) > 2):
        fname_new = sys.argv[2]
        print "New file: %s" % fname_new

    # 3rd argument is the name of the output file.
    if (len(sys.argv) > 3):
        perf_diff_outfile = sys.argv[3]
    print "#  Output file: %s" % perf_diff_outfile

    # Consider any line in the file with the following strings in it as
    # possible data points.
    filter_strings = [ PerfDataPoint.CILKPUB_DATA_PREFIX ]

    # Parse first file if it exists.
    if (fname_base):
        base_data = get_perf_data(fname_base,
                                  perf_data_base_tmp,
                                  [filter_strings])
    # Parse second file if it exists.
    if (fname_new):
        new_data = get_perf_data(fname_new,
                                 perf_data_new_tmp,
                                 [filter_strings])
    # Now compare files.    
    if (fname_base):
        if (fname_new):
            # If there were two files passed in, then diff the two.
            generate_performance_diff(base_data, new_data, perf_diff_outfile)
        else:
            # Otherwise, "diff" the one file with itself.
            generate_performance_diff(base_data, base_data, perf_diff_outfile)

            
# Run if we aren't importing.             
if __name__ == "__main__":
    main()
