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
# \file PerfDataPoint.py
#
# \brief [<i>Performance test script</i>]: Data point type for data
# collected from a performance run.
#
# \author Jim Sukha
# \version 1.02
#

import re;    

## A data point from a performance run.
#
#   A data point contains 6 fields:
#    
#   bench        :  Short string describing the benchmark
#   P            :  Number of processors used
#   time         :  Running time
#   input_params :  Parameters describing the run.
#   output_data  :  Output data associated with the run.
#   count        :  Number of runs added into this data point.  Usually 1,
#   unless this data point represents an average.
#
#   The key for a data point is triple (bench, input_params, P)
#
#   Normally, we expect @c count to be 1, and @c time to represent the
#   time to execute a singel run.  We can also construct "aggregate"
#   data points, however, which represent a combination of multiple
#   data points.
class PerfDataPoint(object):

    # The string we use to match whether a string is a valid cilkpub
    # performance string or not. If the benchmark name contains this
    # string, then we assume it is valid.
    #
    # This test is a little bit of a hack right now.
    # CILKPUB_ID_STRING = ".t.exe"

    ##
    # The string that should be the first element in the
    # comma-separated list for a data points generated for benchmarks
    # in Cilkpub.
    CILKPUB_DATA_PREFIX = "CILKPUB_DATA_POINT"

    # Ways of merging data points with the same key.

    ## Merge type: add times together.
    MERGE_SUM  = 1
    ## Merge type: keep minimum time.
    MERGE_MIN  = 2
    ## Merge type: keep maximum time.
    MERGE_MAX  = 3   
    ## Merge type: keep the last time we saw.
    MERGE_LAST = 4
    ## Merge type: tThrow an error if we see a duplicate data point.
    MERGE_WARN = 5
    

    ## Constructor
    #
    # @param in_bench         Name of benchmark.
    # @param in_P             Number of processors used during execution.
    # @param in_time          Running time.
    # @param in_input_params  String or list representing input parameters.
    # @param in_data          String or list representing output parameters
    # @param in_count         Count for this data point. (Default value of 1)
    def __init__(self,
                 in_bench,
                 in_P,
                 in_time,
                 in_input_params,
                 in_data,
                 in_count=1):
        self.__bench = in_bench
        self.__P = in_P
        self.__time = in_time
        self.__min_time = in_time
        self.__max_time = in_time
        self.__input_params = in_input_params
        self.__output_data = in_data
        self.__count = in_count
        
    ## Tuple representing the key for this point.
    # A point has key made of its benchmark name, input parameters,
    # and value of P.
    def key(self):
        return (self.__bench, self.__input_params, self.__P)

    ## The key for this point, represented as a CSV string.
    def key_string(self):
        return "%s, %s, %.2d" % (self.__bench, self.__input_params, self.__P)

    ## Return the time from this point.
    def get_time(self):
        return self.__time

    ## Return P for this point.
    def get_P(self):
        return self.__P

    ## Return the minimum time from this point.
    def get_min_time(self):
        return self.__min_time

    ## Return the maximum time from this point.
    def get_max_time(self):
        return self.__max_time

    # UNUSED
    # ## Returns the input parameters.
    # def get_input_params(self):
    #     return "%s" % self.__input_params

    ##  Get an "average" time (but throwing out the smallest and
    ##  largest point).
    def get_avg_time_without_minmax(self):
        if self.__count <= 1:
            return self.__time
        elif self.__count == 2:
            return (self.__time - self.__max_time)
        else:
            assert(self.__count >= 3)
            return ((self.__time - (self.__min_time + self.__max_time)) / (self.__count - 2))

    ## Returns true if the benchmark field is filled in.
    def is_valid(self):
        return (self.__bench != None)


    ## Get a string equivalent of this object.
    def get_string(self):
        return "%s, %d, %f, %s, %s, %d" % (self.__bench,
                                           self.__P,
                                           self.__time,
                                           self.__input_params,
                                           self.__output_data,
                                           self.__count)
    ## Print the object
    def print_obj(self):
        print self.get_string()


    ## Average the time in this data point.  (Divide its time by
    # count, reset count to 1).
    def compute_average(self):
        if (self.__count > 0):
            self.__time = self.__time / self.__count
            self.__count = 1
        return self

    ## Compute the average time for this data point, ignoring
    # the minimum and maximum time.
    def compute_average_without_minmax(self):
        if (self.__count > 0):
            tmp_time = self.get_avg_time_without_minmax()
            self.__time = tmp_time
            self.__min_time = tmp_time
            self.__max_time = tmp_time
            self.__count = 1
        return self

    ## Takes a key tuple, and strips out the "P" value from the tuple.
    @staticmethod
    def extract_plot_key(key):
        (b, other, P) = key
        return (b, other)

    ## Takes a key, and generates a more concise string that can be
    #  used to describe the key.
    #
    # This string is hopefully unique, but it is not guaranteed.
    @staticmethod
    def extract_plot_key_string(pkey):
        (b, other) = pkey
        plot_key_string = "%s" % b
        if other:
            plot_key_string = plot_key_string + "_%s" % other

        # # Strip out common test string.
        # plot_key_string = plot_key_string.replace(PerfDataPoint.CILKPUB_ID_STRING, "")

        # Strip out "." or "/" characters, replacing them with '-'.
        plot_key_string = plot_key_string.replace('.', '-')
        plot_key_string = plot_key_string.replace('/', '-')
        plot_key_string = plot_key_string.replace('_', '-')

        # # Eliminate extra consecutive '-' characters.
        # tmp_substrings = plot_key_string.split('-')
        # filtered_tmp = [x for x in tmp_substrings if x != '']
        # plot_key_string = "-".join(filtered_tmp)

        # Filter out everything except letters, digits, '-'
        filtered_string = "".join([x for x in plot_key_string if x.isalpha() or x.isdigit() or x == '-'])
        filtered_string.strip("-")
        return filtered_string

    
    ## Parse a line from a Cilkpub benchmark.
    #
    # The line should have 6 comma-separated values:
    #
    #  "CILKPUB_DATA_POINT, 1.861935, 8, bench, [small, q], [32.43, foo, bar]"
    #
    #  0. Cilkpub data prefix
    #  1. Time (in ms)
    #  2. P
    #  3. Benchmark name
    #  4. List of other parameters
    #  5. List of other outputs
    @staticmethod
    def parse_cilkpub_line(line):

        # A (verbose) regular expression that is looking for the six
        # tokens that we expect.

        cilkpub_pattern = re.compile(r"""
                                     (?P<prefix>[^,]+),\s*            # <prefix>,
                                     (?P<time>[^,]+),\s*              # <time>,
                                     (?P<P>[^,]+),\s*                 # <P>,
                                     (?P<bench>[^,]+),\s*             # <bench>,
                                     \[(?P<input_params>.*)\]\s*,\s*  # <input_params>,
                                     \[(?P<output_params>.*)\]\s*     # <output_params>
                                     (.*)$                            # Everything else
                                     """,
                                     re.VERBOSE)
        
        line_match = cilkpub_pattern.match(line)

        if line_match:
            # print "MATCH:"
            # print line_match
            # print line_match.groups()
            # print "PREFIX: %s" % line_match.group('prefix')
            # print "TIME: %s" % line_match.group('time')
            # print "P: %s" % line_match.group('P')
            # print "bench: %s" % line_match.group('bench')
            # print "input_params: %s" % line_match.group('input_params')

            # print "output_params: %s, len = %d" % (line_match.group('output_params'),
            #                                        len(line_match.group('output_params')))
            if (line_match.group('prefix').strip() == PerfDataPoint.CILKPUB_DATA_PREFIX):
                return PerfDataPoint(line_match.group('bench').strip(),
                                     int(line_match.group('P').strip()),
                                     float(line_match.group('time').strip()),
                                     line_match.group('input_params').strip(),
                                     line_match.group('output_params').strip(),
                                     1)
            else:
                # Return an empty data point.
                print "Line [%s] matched, but prefix does not match." % line
                return None
        else:
            # No match at all.
            return None

    ## Print this line in a cilkpub parseable format.
    def get_cilkpub_line(self):
        return "%s, %f, %d, %s, [%s], [%s]" % (PerfDataPoint.CILKPUB_DATA_PREFIX,
                                               self.__time,
                                               self.__P,
                                               self.__bench,
                                               self.__input_params,
                                               self.__output_data)

    ## Merge this data point with a different one.
    # Both data points should have the same @c key_string
    def merge(self, other_point, merge_type):
        # Report an error if we are trying to merge points with
        # different keys.
        if (self.key_string() != other_point.key_string()):
            print "ERROR: trying to merge other point with key %s into this point with key %s" % (other_point.key_string(), self.key_string())
            return self

        # Keep track of minimum and maximum times.
        if (other_point.__min_time < self.__min_time):
            self.__min_time = other_point.__min_time
        if (other_point.__max_time > self.__max_time):
            self.__max_time = other_point.__max_time
        
        if (merge_type == PerfDataPoint.MERGE_SUM):
            # Merge the time, data, and count by adding.
            self.__time = self.__time + other_point.__time

            # If they are both nonempty strings, join with a ", " so
            # we separate them.
            if (isinstance(self.__output_data, basestring) and
                isinstance(other_point.__output_data, basestring) and
                (len(self.__output_data) > 0) and
                (len(other_point.__output_data) > 0)):
                self.__output_data = self.__output_data + ", " + other_point.__output_data
            else:
                # Otherwise, pretend they are either strings or lists
                # and just concatenate.
                self.__output_data = self.__output_data + other_point.__output_data

            self.__count = self.__count + other_point.__count
        elif (merge_type == PerfDataPoint.MERGE_MIN):
            if (other_point.__time < self.__time):
                self.__time = other_point.__time
                self.__output_data = other_point.__output_data
                self.__count = other_point.__count
        elif (merge_type == PerfDataPoint.MERGE_MAX):
            if (other_point.__time > self.__time):
                self.__time = other_point.__time
                self.__output_data = other_point.__output_data
                self.__count = other_point.__count
        elif (merge_type == PerfDataPoint.MERGE_LAST):
            # Fall-through case: 
            # Just replace the previous point
            self.__time = other_point.__time
            self.__output_data = other_point.__output_data
            self.__count = other_point.__count
        else:
            # Covers case of MERGE_WARN:
            print "WARNING: duplicate data point??"
            print key
            print "Existing point = %s" % self.point_map[key].get_string()
            print "New data point = %s" % other_point.get_string()

        return self


