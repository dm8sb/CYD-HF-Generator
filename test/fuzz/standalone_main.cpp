// Driver for one harness without libFuzzer (-DFUZZ_STANDALONE): MinGW on Windows, gcc, or any
// compiler without -fsanitize=fuzzer. Runs every corpus file once, then N pseudo-random inputs
// with a fixed seed (fuzz_random.h). Not coverage guided; a failed check aborts and prints the
// input. Usage (flags in libFuzzer style, so the same command lines work):
//   fuzz_<target> [-runs=N] [-seed=S] [-max_len=L] [corpus dir or file ...]
// Defaults: -runs=100000 -seed=1 -max_len=1024.

#include "fuzz_common.h"
#include "fuzz_corpus.h"
#include "fuzz_random.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
extern "C" const char* fuzzTargetName();

namespace {

void runOne(const std::string& input) {
    // An exact-size heap copy, as libFuzzer passes it (sanitizers see reads past the end).
    std::vector<uint8_t> copy(input.begin(), input.end());
    const uint8_t* p = copy.empty() ? reinterpret_cast<const uint8_t*>("") : copy.data();
    fuzzCurrent().data = p;
    fuzzCurrent().size = copy.size();
    LLVMFuzzerTestOneInput(p, copy.size());
}

} // namespace

int main(int argc, char** argv) {
    unsigned long runs = 100000;
    unsigned long long seed = 1;
    size_t maxLen = 1024;
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!strncmp(a, "-runs=", 6)) runs = strtoul(a + 6, nullptr, 10);
        else if (!strncmp(a, "-seed=", 6)) seed = strtoull(a + 6, nullptr, 10);
        else if (!strncmp(a, "-max_len=", 9)) maxLen = (size_t)strtoul(a + 9, nullptr, 10);
        else if (a[0] == '-') fprintf(stderr, "ignored option %s\n", a);
        else fuzzCollect(a, files);
    }
    fuzzCurrent().target = fuzzTargetName();

    std::vector<std::string> seeds;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string data;
        if (!fuzzReadFile(files[i], data)) {
            fprintf(stderr, "cannot read %s\n", files[i].c_str());
            return 2;
        }
        runOne(data);
        seeds.push_back(data);
    }

    FuzzRng rng(seed);
    std::string input;
    for (unsigned long r = 0; r < runs; ++r) {
        fuzzMakeInput(rng, seeds, maxLen, input);
        runOne(input);
    }
    printf("%s: %u corpus files and %lu random inputs (seed %llu, max_len %u) passed\n", fuzzTargetName(),
           (unsigned)files.size(), runs, seed, (unsigned)maxLen);
    return 0;
}
