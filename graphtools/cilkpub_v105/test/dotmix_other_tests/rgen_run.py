## rgen_run.py
#
#######################################################################
#  Copyright (C) 2012 Intel Corporation
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


# Simple test script to generate output files of random numbers.
#
# This script currently has two test modes.
#
#  1. The default test is to generate files of random numbers using each
#     of the different RNG's, using different seeds and algorithms.
#     This test will hash each file using md5sum, and check that
#     hashes for a particular generator are unique.
#
#  2. If --run_dieharder is the first argument, then it will generate
#     a file of random numbers and run it through Dieharder.
#     This test generates a large file (e.g. about 15 GB).
#     so check for available disk space before running it...

import os
import sys

VERBOSE = 1

# Declare all the hard-coded values, parameters, etc in this script.
# Some of these values will need to change if names, etc.  change in
# the test system.
TEST_SCRIPT = "dotmix_rgen_driver"
TMP_FOLDER = "tmp"
EXPECTED_FOLDER = "expected"

# All these maps need to match the one in the dotmix_rgen_driver.cpp.
# @todo  It would be nice to keep these synchronized somehow...
ALG_MAP = { 0: "loop",
            1: "tree",
            2: "loopB256",
            3: "fibB8" }

RNG_MAP = { 0: "dmix",
            1: "dmixPrime",
            2: "fdmix",
            3: "fdmixPrime" }

# This format string also needs to match the output in the test
# driver... more ugliness...
RGEN_FILE_FORMAT_STRING = "%s_N%d_A%s_G%s_R%d_Seed%lu.output"


HASH_FILE_PREFIX = "rng_hash_output"
SIZE_FILE = "rng_size_outputs.txt"
DIEHARDER_OUTPUT_FILE = "dieharder_test_results.txt"
DIEHARDER_TEST_LIST = [ (19,    "dieharder_in", 1),    # Ternary tree (3^19)
                        (3**19, "dieharder_in", 0),    # Loop      (3^19)
                        (3**19, "dieharder_in", 2),    # LoopB256  (3^19)
                        (43,    "dieharder_in", 3) ]   # Fib8 40


DEFAULT_DOTMIX_SEED = 0x908e39a34eb8b33L
assert(0x908e39a34eb8b33L == 651020397607357235)  # This happens to be the default seed.

DIEHARDER_SEED_LIST = [ DEFAULT_DOTMIX_SEED,      
                        10416326361717715772 ]

DIEHARDER_MIX_LIST = [4]

FILECHECK_TEST_LIST = [ (13,    "filecheck_in", 1),
                        (3**13, "filecheck_in", 0),
                        (3**13, "filecheck_in", 2),
                        (24,    "filecheck_in", 3) ]
FILECHECK_MIX_LIST = [4, 8, 16]
FILECHECK_SEED_LIST = DIEHARDER_SEED_LIST + [ 2001959028291723867,
                                              3698757827557609530,
                                              4561586318396457410,
                                              9223372036854775807,
                                              0, 1, 3, 32, 1201 ]

# WARNING: 0 appears to be a poor seed for DotMix, in that it doesn't
# generate random numbers that pass Dieharder tests.

##########################################################
# Beginning of test runs
 
run_dieharder = False
if (len(sys.argv) > 1):
    arg = sys.argv[1]
    if arg == "--run_dieharder":
        run_dieharder = True

# Choose the test parameters, based on which test we are running.
if run_dieharder:
    param_test_list = DIEHARDER_TEST_LIST
    mix_list = DIEHARDER_MIX_LIST
    seed_list = DIEHARDER_SEED_LIST
    hash_file_prefix = "dieharder_%s" % HASH_FILE_PREFIX
    size_file = "dieharder_%s" % SIZE_FILE
else:
    param_test_list = FILECHECK_TEST_LIST
    mix_list = FILECHECK_MIX_LIST
    seed_list = FILECHECK_SEED_LIST
    hash_file_prefix = "filecheck_%s" % HASH_FILE_PREFIX
    size_file = "filecheck_%s" % SIZE_FILE

# Clear the hash and size files.
os.system("rm -f %s/%s*" % (TMP_FOLDER, hash_file_prefix))
os.system("rm -f %s/%s" % (TMP_FOLDER, size_file))

# Clear the dieharder test output.
os.system("rm -f %s/%s" % (TMP_FOLDER, DIEHARDER_OUTPUT_FILE))

# Clear out all the data files. 
os.system("rm -f %s/*.output" % TMP_FOLDER)


for rng_type in xrange(0, len(RNG_MAP)):
    rng_hash_file = "%s_%s.txt" % (hash_file_prefix,
                                   RNG_MAP[rng_type])
    for seed in seed_list:
        if VERBOSE >= 1 :
            echo_string = "\"      %s testing seed %d\"" % (RNG_MAP[rng_type],
                                                            seed)
            os.system("echo %s" % echo_string)
            
        for (n, prefix, alg_type) in param_test_list:
            for R in mix_list:
                cmds = []

                # Check for either "foo" or "foo.exe"
                # Yeah, this is not robust...
                rgen_prog = "%s_%d" % (TEST_SCRIPT, R)
                if not os.path.exists("%s/%s" % (TMP_FOLDER, rgen_prog)):
                    rgen_prog = rgen_prog + ".exe"

                rgen_file = RGEN_FILE_FORMAT_STRING % (prefix,
                                                       n,
                                                       ALG_MAP[alg_type],
                                                       RNG_MAP[rng_type],
                                                       R,
                                                       seed)
                
                args = "%d %s %lu %d %d" % (n,
                                            prefix,
                                            seed,
                                            alg_type,
                                            rng_type)

                # Execute the command to generate an output file.
                exec_cmd = "./%s %s" % (rgen_prog, args)

#               cmds.append("echo \".... %s --> %s\"" % (exec_cmd, rgen_file))
                if (VERBOSE >= 2) or run_dieharder:
                    cmds.append("echo \"      %s\"" % rgen_file)
                cmds.append(exec_cmd)

                # Run Dieharder on the test.
                if run_dieharder:
                    dieharder_cmd = "dieharder -a -g 201 -f %s >> %s 2>& 1" % (rgen_file,
                                                                               DIEHARDER_OUTPUT_FILE)
                    cmds.append("echo %s" % dieharder_cmd)
                    cmds.append(dieharder_cmd)

                # Compute a hash of the file for reference.
                ls_cmd = "ls -s %s >> %s" % (rgen_file, size_file)
                cmds.append(ls_cmd)

                # Force reading of the file in binary mode
                # before coming the md5 hash.
                hash_cmd = "md5sum -b %s >> %s" % (rgen_file, rng_hash_file)
                cmds.append(hash_cmd)

                # Remove the output file.
                cmds.append("rm %s" % rgen_file)

                # Actually go through the list of commands we created,
                # and execute them.  Die if any of them fail.
                try :
                    for cmd in cmds:
                        temp_cmd = "cd %s; %s" % (TMP_FOLDER, cmd)
                        p = os.system(temp_cmd)
                        if p != 0:
                            print "ERROR in executing command %s" % temp_cmd
                            exit(1)
                except:
                    print "ERROR encountered in test. Quitting"
                    exit(1)

    # Check that the file of hashes is unique.
    # Technically speaking, this does not have to be true
    # for the same rng tested on different generation algorithms, since
    # two different algorithms could end up generating the same set of pedigrees.
    # But I will assume that is unlikely for now?
    hash_file_check_cmd = "awk '{print $1}' %s/%s | sort | sort -cu" % (TMP_FOLDER, rng_hash_file)

    p = os.system(hash_file_check_cmd)
    if p != 0:
        print "WARNING [%s]: found files with duplicate has in RNG hash file %s" % (hash_file_check_cmd,
                                                                                    rng_hash_file)
        print "Duplication might occur with different algorithms using the same RNG"
        print "but should not occur for the same RNG using different seeds"
    else:
        print "Hash unique check on %s PASSED" % rng_hash_file

    hash_file_expected_cmd = "diff %s/%s %s/%s" % (EXPECTED_FOLDER, rng_hash_file,
                                                   TMP_FOLDER, rng_hash_file)
    p = os.system(hash_file_expected_cmd)
    if p != 0:
        print "WARNING [%s]: rng_hash_file %s does not match expected output..." % (hash_file_check_cmd,
                                                                                    rng_hash_file)
    else:
        print "Hash file %s compared to expected output: PASSED" % rng_hash_file
        

# Check that the size file  is unique.
size_file_check_cmd = "sort %s/%s | sort -cu" % (TMP_FOLDER, size_file)                    
p = os.system(size_file_check_cmd)

if p != 0:
    print "ERROR: found a duplicate file name in RNG file list"
else:
    print "File size check... PASSED" 

