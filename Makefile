CXXFLAGS=-Wall -Wextra -O2 -g
LDLIBS=-lboost_program_options

SRCDIR:=src
OBJDIR:=obj
TESTDIR:=tests

SOURCES:=$(wildcard $(SRCDIR)/*.cpp)
OBJECTS:=$(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))

TESTSOURCES:=$(wildcard $(TESTDIR)/*.cpp)
TESTOBJECTS:=$(patsubst $(TESTDIR)/%.cpp, $(OBJDIR)/%.o, $(TESTSOURCES))

EXECUTABLE=faultsim
TEST=$(TESTDIR)/unit


all: depend $(EXECUTABLE) test

test: $(TEST)
	@./$<

clean:
	@rm -vf $(EXECUTABLE) $(TEST)
	@rm -rvf $(OBJDIR)

depend: $(OBJDIR)/.depend


$(OBJDIR)/.depend: $(SOURCES) $(TESTSOURCES)
	@mkdir -p $(OBJDIR)
	@rm -f $(OBJDIR)/.depend
	@$(foreach SRC, $(filter-out $(TESTSOURCES), $^), \
		$(CXX) $(CXXFLAGS) -I$(SRCDIR) -MM -MT $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC)) $(SRC) >> $(OBJDIR)/.depend ;)
	@$(foreach TEST, $(TESTSOURCES), \
		$(CXX) $(CXXFLAGS) -I$(SRCDIR) -MM -MT $(patsubst $(TESTDIR)/%.cpp, $(OBJDIR)/%.o, $(TEST)) $(TEST) >> $(OBJDIR)/.depend ;)

ifneq ($(MAKECMDGOALS),clean)
-include $(OBJDIR)/.depend
endif


$(EXECUTABLE): $(OBJECTS) | depend
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST): $(filter-out $(OBJDIR)/main.o, $(OBJECTS)) $(TESTOBJECTS) | depend
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJECTS): | $(OBJDIR)

$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(COMPILE.cc) -o $@ $<

$(OBJDIR)/%.o: $(TESTDIR)/%.cpp
	$(COMPILE.cc) -I$(SRCDIR)/ -o $@ $<

.PHONY: all clean depend test
