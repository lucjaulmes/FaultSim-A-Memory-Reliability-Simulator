CXXFLAGS=-Wall -O2 -g
LDLIBS=-lboost_program_options

SRCDIR:=src
OBJDIR:=obj

SOURCES:=$(wildcard $(SRCDIR)/*.cpp)
OBJECTS:=$(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))

EXECUTABLE=faultsim


all: depend $(EXECUTABLE)

clean:
	@rm -vf $(EXECUTABLE)
	@rm -rvf $(OBJDIR)

depend: $(OBJDIR)/.depend


$(OBJDIR)/.depend: $(SOURCES)
	@mkdir -p $(OBJDIR)
	@rm -f $(OBJDIR)/.depend
	@$(foreach SRC, $(SOURCES), $(CXX) $(CXXFLAGS) -MM -MT $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC)) $(SRC) >> $(OBJDIR)/.depend ;)

ifneq ($(MAKECMDGOALS),clean)
-include $(OBJDIR)/.depend
endif


$(EXECUTABLE): $(OBJECTS) $(SRCDIR)/*.hh | depend
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

$(OBJECTS): | $(OBJDIR)

$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(COMPILE.cc) -o $@ $<

.PHONY: all clean depend
