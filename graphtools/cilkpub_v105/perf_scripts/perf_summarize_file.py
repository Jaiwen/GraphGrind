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
# \file perf_summarize_file.py
#
# \brief [<i>Performance test script</i>]: Reads in a file of performance
# data, and merges all data points with the same key together.
#
# \author  Jim Sukha
# \version 1.02


import os
import sys

from PerfDataPoint import *;
from PerfDataSet import *;
import PerfDataParser;

##
# Script to read in input file, and merge all data points with the
# same key together.
#
# Usage: <this script> <infile> [outfile] [merge_type]
#          <infile>     is a required argument."
#          <outfile>    is \"tmp_summary_output.dat\" by default"
#          [merge_type] is \"avg\" by default (can also be \"min\")"
#
def main():
    infile = None
    outfile = "tmp_summary_output.dat"
    merge_type = "avg"

    if (len(sys.argv) > 1):
        infile = sys.argv[1]
    else:
        print "Usage: <this script> <infile> <outfile> <merge_type>"
        print "  <infile>     is a required argument."
        print "  <outfile>    is \"tmp_summary_output.dat\" by default"
        print "  <merge_type> is \"avg\" by default (can be \"min\")"
        exit(0)
        

    if (len(sys.argv) > 2):
        outfile = sys.argv[2]

    if (len(sys.argv) > 3):
        merge_type = sys.argv[3]
        if (merge_type not in { "avg", "min" }):
            merge_type = "avg"


    print "# Input file: %s"  % infile
    print "# Output file: %s" % outfile
    print "# Merge type: %s"  % merge_type

    # Compute a merged input/output file names.
    (infile_head, infile_tail) = os.path.split(infile)
    merged_in_fname = infile_head + merge_type + "_" + infile_tail

    (outfile_head, outfile_tail) = os.path.split(outfile)
    merged_out_fname = outfile_head + merge_type + "_" + outfile_tail
    print "Generating output file %s" % merged_out_fname


    if (merge_type == "min"):
        merge_val = PerfDataPoint.MERGE_MIN
    elif (merge_type == "max"):
        merge_val = PerfDataPoint.MERGE_MAX
    else:
        merge_val = PerfDataPoint.MERGE_SUM

    input_data = PerfDataParser.parse_data_file(infile,
                                                default_desc=merged_in_fname,
                                                merge_type=merge_val,
                                                compute_average=True)
    input_data.output_to_file(merged_out_fname)


# Run if we aren't importing.
# Right now, we can't really import this file...
    
if __name__ == "__main__":
    main()
    
