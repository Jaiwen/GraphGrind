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
# \file DataParser.py
#
# \brief [<i>Performance test script</i>]: Functions for parsing
# output Cilkpub performance-data output into a PerfDataSet object.
#
# \author Jim Sukha
# \version 1.02
#

from PerfDataPoint import *;
from PerfDataSet import *;
                            
##
# Tries to parse a line of performance data into a @c PerfDataPoint
# object.
# 
# This method tries to parse @c line according to the Cilkpub format,
# described in @ref PerfDataPoint.py.
def parse_generic_data_point(line):
    # Try a cilkpub parse first.
    p = PerfDataPoint.parse_cilkpub_line(line)
    if p and p.is_valid():
        return p

    # INSERT OTHER INTERESTING PARSE FORMATS HERE.

    # Failed to parse. return null data point.
    return None

##
# Tries to extract a description from a line of text.
# 
# Look for any line of the form
#
#   @c "X = desc"
#
# where @c X should be a string in @c desc_set and @c desc is the
# description string we want to return.
# 
# The case on "desc" should not matter.
def parse_description_line(line,
                           verbose,
                           desc_set=["version",
                                     "branch"]):
    # Look for a branch name.
    tmp = line.replace("#", " ")
    toks = tmp.split("=")

    try:
        if (len(toks) == 2):
            first_token = toks[0].strip()
            if (first_token.lower() in desc_set):
                desc = toks[1].strip()
                return desc
            if verbose:
                print "# Ignoring line: %s" % tmp
            return None
    except AttributeError:
        return None
    
## Tries to parse a data file, and return a PerfDataSet object.
def parse_data_file(fname, merge_type=PerfDataPoint.MERGE_SUM,
                    default_desc="UNKNOWN",
                    verbose=False,
                    compute_average=False):
    desc = None
    point_list = []

    print "Parsing data file: merge_type is %d" % merge_type
    f = open(fname)
    for line in f:
        p = parse_generic_data_point(line)
        if p and p.is_valid():
            point_list.append(p)
        else:
            # Invalid point.  But look for a comment
            tmp_desc = parse_description_line(line, verbose)

            # If don't have a description yet, set it.
            if desc:
                if tmp_desc and (tmp_desc != desc):
                    print "ERROR: desc = %s. Found another description %s" % (desc, tmp_desc)
            else:
                if tmp_desc:
                    desc = tmp_desc

    if not desc:
        desc = default_desc
    print "# Parsed file %s: read in %d points" % (fname, len(point_list))
    ans =  PerfDataSet(desc, point_list, merge_type)

    if compute_average:
        ans.compute_average()
        
    return ans


