# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -I$(realpath $(CURDIR)/../include) $(shell root-config --cflags)
LDFLAGS = $(shell root-config --libs)

# Project directory structure
SRCDIR = src
BINDIR = bin

# Source files
SRCS = $(SRCDIR)/integralEvents.cpp

# Object files - we'll keep them in src for simplicity
OBJS = $(SRCS:.cpp=.o)

# Target executables - placed in bin
TARGETS = $(BINDIR)/integralEvents

# Default target
all: $(TARGETS)

# Generic rule for compiling C++ files to object files
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link object files to create executables

$(BINDIR)/integralEvents: $(SRCDIR)/integralEvents.o | $(BINDIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Create bin directory if it doesn't exist
$(BINDIR):
	mkdir -p $(BINDIR)

# Clean up - remove all generated files in src and bin
clean:
	@echo "Cleaning up..."
	rm -f $(OBJS) $(TARGETS)
	rm -rf $(BINDIR)

# Phony targets
.PHONY: all clean