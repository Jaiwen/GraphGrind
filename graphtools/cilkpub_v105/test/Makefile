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

# Test all modules, including unreleased code.
# 
# To use this Makefile, copy this file to cilkpub/test

# Test names: For every item "x" in this list, the Makefile
# expects a test file of the form "x_test.cpp"
TEST_NAMES = sort scan scan_pack pedigrees dotmix_pi creducer_opadd_array \
reducer_vector_simple

# Other test folders. For every item "y" in this list,
# the Makefile expects a test folder with name
#  "y_other_tests"
# 
# This folder should have a Makefile with its own
#  "make clean" and "make run_all" targets.
OTHER_TEST_DIRS = dotmix

# Names of performance tests.  For every item "x" in this list, the
# Makefile expects a test named "test_x.cpp".
# A test can be both a normal test and a performance test.
#
# Performance tests should take in an extra argument.
PERFTEST_NAMES = sort

# Includes tests for modules under development.
include mk/dev.mk

include mk/common_suffix.mk

