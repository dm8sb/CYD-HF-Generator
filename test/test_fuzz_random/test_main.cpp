// Fixed-seed random test of the fuzz harnesses (W-S25) in the host envs, also native-win
// without libFuzzer: every harness in test/fuzz runs its seed corpus and RANDOM_INPUTS
// pseudo-random inputs (fuzz_random.h, seed 1). The harnesses check their invariants
// themselves; a broken one aborts with the input printed in hex (test/fuzz/README.md).

#define FUZZ_REGISTRY 1

#include "../fuzz/fuzz_line_framer.cpp"
#include "../fuzz/fuzz_net_settings.cpp"
#include "../fuzz/fuzz_remote_frequency.cpp"
#include "../fuzz/fuzz_sanitize.cpp"
#include "../fuzz/fuzz_scan_list.cpp"
#include "../fuzz/fuzz_scpi.cpp"
#include "../fuzz/fuzz_scpi_number.cpp"

#include "../fuzz/fuzz_corpus.h"
#include "../fuzz/fuzz_random.h"

#include <unity.h>

static const unsigned long RANDOM_INPUTS = 100000;
static const uint64_t SEED = 1;
static const size_t MAX_LEN = 1024;

void setUp() {}
void tearDown() {}

static void runInput(const FuzzTarget& t, const std::string& input) {
    std::vector<uint8_t> copy(input.begin(), input.end());
    const uint8_t* p = copy.empty() ? reinterpret_cast<const uint8_t*>("") : copy.data();
    fuzzCurrent().data = p;
    fuzzCurrent().size = copy.size();
    t.fn(p, copy.size());
}

static void runTarget(const char* name) {
    const FuzzTarget* t = fuzzTargets();
    while (t && strcmp(t->name, name) != 0) t = t->next;
    TEST_ASSERT_NOT_NULL_MESSAGE(t, name);
    fuzzCurrent().target = t->name;

    // pio test runs in the project directory.
    std::vector<std::string> files;
    fuzzCollect(std::string("test/fuzz/corpus/") + name, files);
    TEST_ASSERT_TRUE_MESSAGE(!files.empty(), "seed corpus test/fuzz/corpus/<target> not found");
    std::vector<std::string> seeds;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string data;
        TEST_ASSERT_TRUE(fuzzReadFile(files[i], data));
        runInput(*t, data);
        seeds.push_back(data);
    }
    FuzzRng rng(SEED);
    std::string input;
    for (unsigned long r = 0; r < RANDOM_INPUTS; ++r) {
        fuzzMakeInput(rng, seeds, MAX_LEN, input);
        runInput(*t, input);
    }
    fuzzCurrent().data = nullptr;
}

static void test_line_framer() { runTarget("line_framer"); }
static void test_sanitize() { runTarget("sanitize"); }
static void test_scpi() { runTarget("scpi"); }
static void test_scpi_number() { runTarget("scpi_number"); }
static void test_remote_frequency() { runTarget("remote_frequency"); }
static void test_net_settings() { runTarget("net_settings"); }
static void test_scan_list() { runTarget("scan_list"); }

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_line_framer);
    RUN_TEST(test_sanitize);
    RUN_TEST(test_scpi);
    RUN_TEST(test_scpi_number);
    RUN_TEST(test_remote_frequency);
    RUN_TEST(test_net_settings);
    RUN_TEST(test_scan_list);
    return UNITY_END();
}
