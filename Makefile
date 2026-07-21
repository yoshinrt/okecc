# -*- tab-width: 4 -*-

.SECONDARY:

SRCDIR		= ./okecc
OBJDIR		= ./obj
CXXFLAGS	= -pipe -O3 -march=native -mtune=native -ffast-math -flto=auto -pthread -std=c++20 -I$(SRCDIR)
HEADERS		= $(SRCDIR)/okecc.h $(SRCDIR)/opt_coordinate.h 

PCH_SRC		= $(SRCDIR)/okecc.h
PCH_OUT		= $(PCH_SRC).gch

# CHP
CXXFLAGS	+= -DCHP

ifdef DEBUG
	CXXFLAGS	+= -D_DEBUG
else
	MAKEFLAGS	+= -j4
endif

$(PCH_OUT): $(PCH_SRC) $(SRCDIR)/opt_coordinate.h
	g++ $(CXXFLAGS) -x c++-header $< -o $@

$(OBJDIR)/%.o: %.cpp $(PCH_OUT)
	mkdir -p $(OBJDIR)
	g++ $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/okecc_sa.o: $(SRCDIR)/okecc_sa.cpp $(PCH_OUT)
	mkdir -p $(OBJDIR)
	g++ $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%: $(OBJDIR)/%.o $(OBJDIR)/okecc_sa.o
	g++ $(CXXFLAGS) $^ -o $(OBJDIR)/$*

%.oke: $(OBJDIR)/%
	$(OBJDIR)/$* || true
	if [ -e okecc.svg ]; then mv okecc.svg $*.svg; fi

clean:
	rm -rf *.svg $(OBJDIR) build x64 $(PCH_OUT)
