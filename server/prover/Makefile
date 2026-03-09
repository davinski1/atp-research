CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall

all: resolution fol_prover

resolution: resolution.cpp
	$(CXX) $(CXXFLAGS) -o resolution resolution.cpp

fol_prover: fol_prover.cpp
	$(CXX) $(CXXFLAGS) -o fol_prover fol_prover.cpp

clean:
	rm -f resolution fol_prover

.PHONY: all clean
