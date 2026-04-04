# -*- tab-width: 4 -*-

.SECONDARY:

SRCDIR		= ./okecc
OBJDIR		= ./obj
CXXFLAGS	= -O3 -march=native -mtune=native -ffast-math -flto=auto -std=c++20 -I$(SRCDIR)
HEADERS		= $(SRCDIR)/okecc.h $(SRCDIR)/mcmgr.h

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
	if [ -e chip.svg ]; then mv chip.svg $*.svg; fi

clean:
	rm -rf *.svg $(OBJDIR)
