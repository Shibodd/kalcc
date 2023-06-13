OBJS = parser.o driver.o scanner.o main.o ast.o
DEPS := $(OBJS:.o=.d)

-include $(DEPS)

LLVM_VERSION = 14

CXX = clang++-$(LLVM_VERSION)
CXXFLAGS = $(shell llvm-config-$(LLVM_VERSION) --cxxflags --system-libs) -g3 -Og -MMD -fexceptions -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING
LDFLAGS = $(shell llvm-config-$(LLVM_VERSION) --ldflags --libfiles --system-libs)

parser.cc parser.hh: parser.yy
	bison parser.yy -o parser.cc

scanner.cc: scanner.ll
	flex -o scanner.cc scanner.ll

kalcc: $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@ 

clean:
	rm -f parser.cc parser.hh scanner.cc location.hh kalcc $(OBJS) $(OBJS:.o=.d)