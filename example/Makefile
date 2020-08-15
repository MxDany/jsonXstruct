TEST_FILE := $(patsubst %.c,%,$(wildcard *.c))
TEST_OUTPUT := $(patsubst %.c,%_out.json,$(wildcard *.c))
.PHONY: clean tests
LDFLAGS	:= -L$(CURDIR)/../
LDLIBS	:= -ljsonXstruct -ljson-c -lm
OTHER_FLAGS:=-I../deps/include/json-c -L../deps/lib
tests: $(TEST_FILE)
clean:
	-$(RM) $(TEST_FILE)
	-$(RM) $(TEST_OUTPUT)

# Pattern Rule
%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(OTHER_FLAGS) $< $(LDLIBS) -o $@
%: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(OTHER_FLAGS) $< $(LDLIBS) -o $@
%: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(OTHER_FLAGS) $< $(LDLIBS) -o $@
%: %.cxx
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(OTHER_FLAGS) $< $(LDLIBS) -o $@
%: %.c++
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(OTHER_FLAGS) $< $(LDLIBS) -o $@
