# Compiler and flags
CXX = g++

# Optimization flags for fast execution
OPTFLAGS = -O3 -march=native -flto -DNDEBUG

CXXFLAGS = -std=c++17 -Wall -Wextra $(OPTFLAGS) -I. -I$(realpath $(CURDIR)/../include) $(shell root-config --cflags)
LDFLAGS = $(OPTFLAGS) $(shell root-config --libs)

# Project directory structure
SRCDIR = src
BINDIR = bin


# Source files
SRCS = $(SRCDIR)/deserializeBin.cpp $(SRCDIR)/integralEvents.cpp $(SRCDIR)/displayEvents.cpp $(SRCDIR)/integralEvents_strict_doublePeaks.cpp $(SRCDIR)/integralEvents_v2.cpp

# Object files - we'll keep them in src for simplicity
OBJS = $(SRCS:.cpp=.o)

# Target executables - placed in bin
TARGETS = $(BINDIR)/deserializeBin $(BINDIR)/integralEvents $(BINDIR)/displayEvents $(BINDIR)/integralEvents_strict_doublePeaks $(BINDIR)/integralEvents_v2

# Default target
all: $(TARGETS)

# Generic rule for compiling C++ files to object files
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@


# Link object files to create executables
$(BINDIR)/deserializeBin: $(SRCDIR)/deserializeBin.o | $(BINDIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BINDIR)/integralEvents: $(SRCDIR)/integralEvents.o | $(BINDIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BINDIR)/displayEvents: $(SRCDIR)/displayEvents.o | $(BINDIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BINDIR)/integralEvents_strict_doublePeaks: $(SRCDIR)/integralEvents_strict_doublePeaks.o | $(BINDIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BINDIR)/integralEvents_v2: $(SRCDIR)/integralEvents_v2.o | $(BINDIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Create bin directory if it doesn't exist
$(BINDIR):
	mkdir -p $(BINDIR)

# Clean up - remove all generated files in src and bin
clean:
	@echo "Cleaning up..."
	rm -f $(SRCDIR)/*.o $(TARGETS)
	rm -rf $(BINDIR)

# Phony targets
.PHONY: all clean