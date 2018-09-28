
##############################################################
# Code under development.
#
# Strip out this section before we package and release.
ifeq ($(OS),Windows_NT)
  # Windows build
  INCLUDE_FLAGS += /I"$(CILKPUB_TOP)/include_dev"
  CXXFLAGS+= /DCILKPUB_DETRED_DBG_LEVEL=0 
else
  # Unix build
  INCLUDE_FLAGS += -I$(CILKPUB_TOP)/include_dev
  CXXFLAGS+= -DCILKPUB_DETRED_DBG_LEVEL=0 
endif
DEV_TEST_NAMES = rank_range_tag detred_iview_ranks detred_iview	\
detred_opadd fib_det_opadd loopsum_opadd
##############################################################
