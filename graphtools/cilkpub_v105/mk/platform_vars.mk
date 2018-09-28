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

# Sets various flags and variables in Makefile, depending on the
# platform that we are running on.

# Expects $(CILKPUB_DIR) and $(TMP_FOLDER) to be defined.
#
# Checks for $(HAVE_CPP11) variable.

ifeq ($(OS),Windows_NT)
  # Windows build
  CC=icl
  CXX=icl
  INCLUDE_FLAGS += /I"$(CILKPUB_DIR)"
  CXXFLAGS+= /Wall  
  ifeq ($(HAVE_CPP11), 1)
     CXXFLAGS += /Qstd=c++11 
  endif

  ifeq ($(DEBUG), 1)
     CXXFLAGS += /Zi
  else
     CXXFLAGS += /O3
  endif

  #Files generated when building with debugging.
  TMP_DEBUG= *.pdb *.ilk *.obj

  # Hack for distinguishing between a Cygwin and GNU Make environment.
  ifeq ($(SHELL),sh.exe)
     # Assume GNU Make environemnt.
     # Use Windows command-prompt commands. 
     RM_CMD=del /Q
     MKDIR_CMD=mkdir 
     # Separator for multiple commands in one shell line.
     AND_SEP=&&

     # Slashes for subdirectories.  Usually Windows is ok
     # with either direction, but in some places it does not
     # seem happy.   This is even more of a hack...
     SLASH=\

     ICL_STRING=$(shell where icl)
     CILKPUB_STRING=$(shell where cilkrts20.dll)
     $(warning "ICL_STRING is $(ICL_STRING), CILKPUB_STRING is $(CILKPUB_STRING)")
     CILKPUB_OS="WindowsNative"
  else
     # Assume we are probably in a Cygwin environment.
     RM_CMD=rm -rf
     MKDIR_CMD=mkdir -p
     # Separator for multiple commands in one shell line.
     AND_SEP=;
     SLASH=/

     ICL_STRING=$(shell which icl)
     CILKPUB_STRING=$(shell which cilkrts20.dll)
     $(warning "ICL_STRING is $(ICL_STRING), CILKPUB_STRING is $(CILKPUB_STRING)")
     CILKPUB_OS="WindowsCygwin"
  endif
else
  # Unix build
  # If the CILKPLUS_GCC variable is set, use gcc instead.
  ifeq ($(CILKPLUS_GCC), 1)
     CC=gcc
     CXX=g++
     LDFLAGS += -lcilkrts
     CXXFLAGS += -fcilkplus -DCILKPLUS_GCC=1
  else ifeq ($(CILKPLUS_CLANG), 1)
     CC=clang
     CXX=clang
     LDFLAGS += -lcilkrts
     CXXFLAGS += -fcilkplus -DCILKPLUS_CLANG=1
  else
     CC=icc
     CXX=icpc
  endif
  INCLUDE_FLAGS+= -I$(CILKPUB_DIR)
  CFLAGS = -Wall
  CXXFLAGS+= -Wall 

  ifeq ($(DEBUG), 1)
     CFLAGS += -g -O0
     CXXFLAGS += -g -O0
  else
     CFLAGS += -g -O3
     CXXFLAGS += -g -O3
  endif

  ifeq ($(HAVE_CPP11), 1)
     CXXFLAGS += -std=c++0x
  endif

  MKDIR_CMD=mkdir -p
  RM_CMD=rm -rf
  # Separator for multiple commands.
  AND_SEP=;
  # Separator for subdirectories.  Watch out for extra spaces.
  SLASH=/

  CILKPUB_OS="Unix"
endif

