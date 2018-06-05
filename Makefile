#define the long 64--bit for large scale graph
INTT = -DLONG 
PCC = /var/shared/gcc-7.3.0/bin/g++
#PCC = icc
PCFLAGS = -fcilkplus -lcilkrts -O3 -DCILK=1 $(INTT) $(INTE) -I ./cilk-swan/include -L ./cilk-swan/lib -ldl  
COMMON=papi_code.h utils.h IO.h parallel.h gettime.h quickSort.h parseCommandLine.h mm.h partitioner.h graph-numa.h ligra-numa.h

ALL= BFS BC Components PageRank PageRankDelta BellmanFord SPMV BP PageRankBit PageRankConverage
#csc and coo mix, csc for less partition, coo for more partition, inner threshold is GA.m/2
#HILBERT=0, COO will use COO_CSR. For VEBO graph , COO_CSR is faster choice.
LIBS_I_NEED= -DEDGES_HILBERT=1

all: $(ALL)

#other option
CLIDOPT += -std=c++14
#CACHE collection, if PAPI_CACHE=1 collect and print the values
CACHEOPT += -lpapi -DPAPI_CACHE=0
# not use atomic for forward and coo 
SEQOPT += -DPART96=1 
# NUMA special allocation
NUMAOPT += -DNUMA=1 -lnuma 
#REUSE measurement, inital use reuse should not numa allocation
REUSEOPT += -DREUSE_DIST=0
#When using clang:
# OPT += -Wall -Wno-cilk-loop-control-var-modification

% : %.C $(COMMON)
	$(PCC) $(PCFLAGS) $(CFLAGS) $(NUMAOPT) $(CLIDOPT) $(SEQOPT) $(OPT) $(CACHEOPT) -o $@ $< $(LIBS_I_NEED)

.PHONY : clean

clean :
	rm -f *.o $(ALL)
