# -*- tab-width: 4 -*-

.SECONDARY:

SRCDIR		= ./okecc
OBJDIR		= ./obj
CXXFLAGS	= -O3 -march=native -mtune=native -ffast-math -flto=auto -pthread -std=c++20 -I$(SRCDIR)
HEADERS		= $(SRCDIR)/okecc.h
MAKEFLAGS	+= -j4

$(OBJDIR)/%.o: %.cpp $(HEADERS)
	mkdir -p $(OBJDIR)
	g++ $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/okecc_sa.o: $(SRCDIR)/okecc_sa.cpp $(HEADERS)
	mkdir -p $(OBJDIR)
	g++ $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%: $(OBJDIR)/%.o $(OBJDIR)/okecc_sa.o
	g++ $(CXXFLAGS) $^ -o $(OBJDIR)/$*

%.oke: $(OBJDIR)/%
	$(OBJDIR)/$*
	if [ -e okecc.svg ]; then mv okecc.svg $*.svg; fi

clean:
	rm -rf *.svg $(OBJDIR) build x64
