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
# \file PerfDataSet.py
#
# \brief [<i>Performance test script</i>]: A PerfDataSet is a map of
# PerfDataPoint objects, indexed by the keys of the point.
#
# \author Jim Sukha
# \version 1.02
#

from PerfDataPoint import *;

## A PerfDataSet is a map of PerfDataPoint objects, indexed by the keys of the
#  point.
class PerfDataSet(object):
    
    ## Construct a data set from a list of points.
    #
    # @param input_desc    String describing the data set.
    # @param point_list    List of input PerfDataPoint objects.
    # @param merge_type    Process for merging duplicate points.
    def __init__(self,
                 input_desc,
                 point_list,
                 merge_type=PerfDataPoint.MERGE_SUM):
        self.__desc=input_desc

        ## The map for this object, mapping keys to PerfDataPoint objects.
        self.point_map = {}
        for p in point_list:
            if (p.is_valid()):
                key = p.key()
                if key in self.point_map:
                    old = self.point_map[key]
                    self.point_map[key] = old.merge(p, merge_type)
                else:
                    self.point_map[key] = p
            else:
                print "ERROR: found invalid point"
        # print "After finish: we have %d points" % len(self.point_map)

    ## Return the description string of this set.
    def get_desc(self):
        return self.__desc

    ## Merge another data set into this one.
    def merge(self, other_bdata, merge_type=PerfDataPoint.MERGE_SUM):
        if (self.__desc != other_bdata.__desc):
            print "self.__desc is %s, other_bdat.__desc is %s" % (self.__desc,
                                                                  other_bdata.__desc)
        assert(self.__desc == other_bdata.__desc)

        # Scan through the other map, merging any point into this
        # map.
        for key in other_bdata.point_map:
            if key in self.point_map:
                # Point is in both maps. Then merge.
                tmp = self.point_map[key].merge(other_bdata.point_map[key],
                                                merge_type)
                self.point_map[key] = tmp
            else:
                # Point is only in other.  Just create a new point
                # in self.
                self.point_map[key] = other_bdata.point_map[key]

    ## Average each element in the dataset.
    def compute_average(self):
        for key in self.point_map:
            old = self.point_map[key]
            self.point_map[key] = old.compute_average()

    ## Average each element in the data set, ignoring min and max
    ## values.
    def compute_avg_without_minmax(self):
        for key in self.point_map:
            old = self.point_map[key]
            self.point_map[key] = old.compute_average_without_minmax()

    ## Print the data set out to a file.
    def output_to_file(self, fname):
        f = open(fname, "w")
        f.write("# Branch = %s\n" % self.__desc)

        sorted_keys = sorted(self.point_map.keys())

        for key in sorted_keys:
            if self.point_map[key]:
                line = self.point_map[key].get_cilkpub_line()
                print "%s\n" % line
                f.write("%s\n" % line)
            else:
                print "ERROR: key without a corresponding data point..."
                assert(0)

        f.close()
            
