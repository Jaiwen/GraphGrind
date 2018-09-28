include parallelDefs

PLFLAGS += -g
PCFLAGS += -g

PCFLAGS += -I./cilkpub_v105/include
PCFLAGS += -DLONG

COMMON = dataGen.h utils.h IO.h parseCommandLine.h graph.h graphIO.h graphUtils.h parallel.h sequence.h blockRadixSort.h deterministicHash.h transpose.h
GENERATORS = VEBO GaloisToadj addAdjWeights adjToEdgeArray adjTranspose SNAPtoAdj addGaloisWeights

.PHONY: all clean
all: $(GENERATORS)


% : %.C $(COMMON)
	$(PCC) $(PCFLAGS) $(PLFLAGS) -o $@ $< $(LIBS_I_NEED)

clean :
	rm -f *.o $(GENERATORS)
	make clean -s -C data

cleansrc : 
	make -s clean
	rm -f $(COMMON) 
