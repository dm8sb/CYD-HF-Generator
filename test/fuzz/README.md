# Fuzzing

Fuzz harnesses for the pure parsers that take foreign input (W-S25 in `ARBEITSPLAN.md`). Each
harness is a libFuzzer target (`LLVMFuzzerTestOneInput`) that checks its invariants itself with
`FUZZ_CHECK` (message, input in hex, `abort()`), so a broken invariant counts as a crash just like
an ASan or UBSan report. The harnesses are C++11 and build three ways:

| Build | Where | What it is |
|---|---|---|
| libFuzzer + ASan/UBSan | `test/fuzz/build.sh`, Linux, clang | coverage-guided, CI job `Fuzz` |
| Standalone driver | `test/fuzz/build.sh --standalone`, gcc, MinGW, clang | `standalone_main.cpp` (`-DFUZZ_STANDALONE`): seed corpus, then N pseudo-random inputs with a fixed seed |
| PlatformIO suite | `test/test_fuzz_random` in every native env | all harnesses in one program (`-DFUZZ_REGISTRY`), seed corpus plus 100 000 fixed-seed inputs per target (W-S25 in `native-win`) |

## Targets

| Target | Code under test | Input | Invariants |
|---|---|---|---|
| `line_framer` | `LineFramer::feed`, `looksLikeHttp`, `framerErrorCode` | raw connection bytes | a line is ≤ 256 bytes, NUL-terminated, only printable ASCII and TAB, not blank, ≤ 16 program units; events only on LF; error codes match the event; `looksLikeHttp` agrees with an independent matcher of the two W-S18 patterns and never reads past `len`; `reset()` behaves like a new framer |
| `sanitize` | `Sanitize::text` | `[maxBytes][outSize]` + text | never writes past `outSize`; result ≤ min(maxBytes, outSize − 1), NUL-terminated, strictly valid UTF-8 without C0/DEL/C1; idempotent; valid text that fits comes back unchanged; a smaller limit cuts the full result only before a character that does not fit |
| `scpi` | `ScpiSession::execute` behind the `LineFramer` (scpi-parser, `blankAfterInvalid`, all handlers) | `[flags][seed ×4][variant]` + connection bytes | commands carry the session's source; channels, enum values and frequencies in range (W-S19); ≤ 16 commands and ≤ 5120 bytes of printable output ending in LF per framed line; error queue ≤ 16 entries and drains to `0,"No error"` |
| `scpi_number` | `ScpiNumber::decimal`, `ScpiNumber::frequency` | `[scale][unit table]` + token | against an independent reference (grammar + `long double`): `Ok` only for the grammar, `Syntax` only with > 18 significant digits, the value within ½ of the exact one, `Range` only outside int64, never `INT64_MIN`; nothing written on failure; `frequency` = `decimal` with HZ/KHZ/MHZ plus the range |
| `remote_frequency` | `FrequencyUtils::parseRemoteFrequency` | C string | true only in 8 kHz … 160 MHz, value = the number read independently rounded half up; no sign, exponent or hex; blanks around change nothing; false leaves `*out` alone |
| `net_settings` | `NetConfig::parseIpv4`/`formatIpv4`, `checkStaticIp`, `setNetwork`, `wifiQrText`, name and passphrase checks | `[op]` + data | `parseIpv4` accepts exactly the canonical quads (round trip both ways); `checkStaticIp` Ok exactly per W-S12; `setNetwork` stores exactly what it accepted and nothing otherwise; `wifiQrText` never writes past `outSize` and its fields unescape to the input; validators agree with independent checks |
| `scan_list` | `ScanList::add` | records `[len/flags][rssi][ssid]` | ≤ 15 entries sorted by RSSI, one per SSID with its strongest RSSI, exactly the strongest 15 of a model; `shown` = `Sanitize::text(ssid)` |
| `request_guard` | `RequestGuard::check`, login lock, sessions, `scanAllowed`, `checkSecrets`, `captiveLocation`, `WebPassword::valid` | `[config ×6]` + script: HTTP requests (request line, headers, body) and `!` operations (login, wait, logout, scan) | every decision equals an independent reference of the documented order (503, 414 URL > 256 B, 431 headers > 2 KB, 421/302 Host allowlist without IPv6, captive 302 only for page GET/HEAD over the AP, 405, 415, 400/413 Content-Length > 1 KB, 400 broken Cookie/Authorization, 400 upgrade, 403 Origin/Sec-Fetch-Site/token, 411, 401) and does not change with the case of or blanks around values HTTP compares case-insensitively; a session only with the cookie of a live session the guard issued (≤ 4, LRU); Set-Cookie exact; the login lock follows a W-S11 model and never shortens or grows within its window; scan ≤ every 10 s; W-S1 403 exactly over an open AP |
| `rest_json` | `parseChannel`, `parseCh3`, `parseConfig`, `parseAction`, `parseLogin`, `carriesSecret`, `buildState`, `errorJson`, `fromOutcome`, `channelFromPath` | `[function/flags]` + body, or snapshot bytes | against an independent RFC 8259 parser (strings decoded as ArduinoJson does): 413/-223, -102 invalid JSON and duplicate keys, -104 types, -108 unknown keys, -109 no key, -222, -224, 403/-221, 404/-114, first problem in key order; Ok gives exactly the expected commands (≤ 8, documented order, source Web, values inside their types); `carriesSecret` finds pass/psk/secret/key at any depth of what ArduinoJson keeps; `errorJson` fits; the state JSON fits `STATE_SIZE` with device values, parses again, strict UTF-8 without control characters, foreign strings = `Sanitize::text`, values as in the snapshot, never written past its buffer |

The SCPI backend in `fuzz_scpi.cpp` is a fake (no `AppController`): it checks every command it
gets and answers with outcomes drawn from the seed bytes (refused, rate limited, every `Result`,
no snapshot), so all answer paths and query handlers are reached (99 % of the lines of
`remote/scpi.cpp` with 20 000 standalone inputs, gcov). Flag bit 0 switches to raw lines (split at LF
only, no framer) to test the session beyond the framer's guarantees.

Seed corpora are in `corpus/<target>/` (`.gitattributes` keeps them byte-exact), the SCPI lines
taken from `USER_MANUAL_EN.md` section 15, the requests and bodies from `ARBEITSPLAN.md` 3.6,
`test_http_guard`, `test_rest_json` and `tools/web_mock.py`. `scpi.dict` is the libFuzzer
dictionary for `scpi` and `scpi_number`, `request_guard.dict` and `rest_json.dict` those of the
web targets.

`request_guard` reads a script: after six configuration bytes (password, loginForReads, own
addresses, open AP, captive DNS, connection over the AP, start time, seed) each line starting
with `!` is an operation (`!ok [x]`, `!bad`, `!try`, `!wait <ms>`, `!logout <cookie>`,
`!endall`, `!scan`, `!pw <text>`), any other line starts a request whose headers end at an empty
line; `$T` stands for the token, `$S<k>` for the id of the k-th session issued, `X-Heap`,
`X-Pad`, `X-Peer` and `X-Local` set what the server would fill in. The clock advances at most
2^31 ms per input (see the known findings). `rest_json` checks the state JSON in two modes: with
values the device can produce it must fit `STATE_SIZE`; with arbitrary values (64-bit
frequencies, huge numbers) `buildState` only has to give 0 when it does not fit.

## Running locally

Linux with clang (libFuzzer, ASan, UBSan); `build.sh` fetches scpi-parser at the commit pinned in
`platformio.ini` into `.pio/fuzz/scpi-parser` (or set `SCPI_DIR`) and ArduinoJson at the commit
of tag v7.4.3 into `.pio/fuzz/ArduinoJson` (or set `AJSON_DIR`, e.g. to
`.pio/libdeps/native-win/ArduinoJson`):

```bash
test/fuzz/build.sh                      # all targets -> .pio/fuzz/fuzz_<target>
test/fuzz/build.sh scpi                 # one target
mkdir -p /tmp/scpi && cp test/fuzz/corpus/scpi/* /tmp/scpi/
.pio/fuzz/fuzz_scpi -dict=test/fuzz/scpi.dict -max_len=1024 -runs=10000000 /tmp/scpi
.pio/fuzz/fuzz_scpi crash-<sha1>        # reproduce a finding
```

Pass a copy of the corpus directory: libFuzzer writes new inputs into the first directory.

Windows (Git Bash) with PlatformIO's MinGW (GCC 5.1, so C++11; `-static` avoids runtime DLLs):

```bash
export PATH="$HOME/.platformio/packages/toolchain-gccmingw32/bin:$PATH"
test/fuzz/build.sh --standalone
.pio/fuzz/fuzz_scpi.exe -runs=100000 -seed=1 test/fuzz/corpus/scpi
```

The standalone driver accepts `-runs=N` (default 100 000), `-seed=S` (default 1) and
`-max_len=L` (default 1024) and ignores other libFuzzer flags. On Linux, `SANITIZE=1
test/fuzz/build.sh --standalone` adds ASan/UBSan to a gcc build. The PlatformIO suite runs with
the host tests: `pio test -e native-win -f test_fuzz_random`.

## CI budgets

`.github/workflows/fuzz.yml`:

- Every push and pull request: each target 60 s under libFuzzer (`-max_total_time=60`, one
  matrix job per target), plus the standalone driver built with gcc in C++11 with ASan/UBSan
  (seed corpus and 100 000 inputs per target). The job summary lists the executions.
- Release (W-S25): start the workflow by hand (`workflow_dispatch`) with `runs` ≥ 10 000 000
  (default). Each target then runs `-runs=<runs>`; findings (`crash-*`, `timeout-*`, `oom-*`,
  `leak-*`) and the log are uploaded as `findings-<target>` artifacts when a job fails, the grown
  corpus always as `corpus-<target>`.
- `ci.yml` runs `test_fuzz_random` with the other host tests (`native`, `native-cpp20`).

## Adding a target

1. Write `test/fuzz/fuzz_<target>.cpp` like the others: includes `fuzz_common.h`, everything in
   `namespace fuzz_<target>`, one `int run(const uint8_t*, size_t)` that checks the invariants
   with `FUZZ_CHECK`, and `FUZZ_TARGET(<target>, fuzz_<target>::run)` as the last line. Keep it
   C++11 and free of LVGL/Arduino-only code (the fakes in `test/fakes` are on the include path).
2. Add the target to `sources()` and `ALL_TARGETS` in `build.sh` (with the `src/` files it
   needs; further libraries fetched at their pinned commit like scpi-parser and ArduinoJson).
3. Add seeds in `corpus/<target>/` (and a dictionary if the format has keywords).
4. Add `#include "../fuzz/fuzz_<target>.cpp"` and a `RUN_TEST` to
   `test/test_fuzz_random/test_main.cpp`; the source must be in `build_src_filter` of
   `[native_test]` in `platformio.ini` (it is, as soon as the host tests use it).
5. Add the target to both matrix lists in `.github/workflows/fuzz.yml` (and its dictionary to
   the `case` there).

## Known findings

- Fixed in S57b: `ScpiNumber::decimal` capped the exponent at four digits but counted every
  leading zero of the fraction, so `0.` + 1216 zeros + `1E12345` gave `Ok` with 10^17. The
  exponent now saturates above the token length; `scpi_number` checks every length against
  the reference.
- Open (S57c, not fixed here): `RestJson::carriesSecret` misses a secret key inside a value that
  a later duplicate key replaces, because ArduinoJson keeps only the last value of a repeated
  key: `{"a":{"password":"x"},"a":1}` gives false (`corpus/rest_json/secret_overwritten_duplicate`
  reproduces it with `EXTRA_FLAGS=-DFUZZ_KNOWN_FINDINGS=1`). The request endpoints refuse
  top-level duplicates with -102 anyway; nested objects (a future `/api/wifi`) would pass.
- Open (S57c): `RequestGuard` compares times with unsigned 32-bit differences, so after
  `millis()` wraps (49.7 days) an old, never replaced session and an old login lock count again
  until the next 12 h / lock window passes. The harness keeps each input below 2^31 ms.
- Noted (S57c): ArduinoJson refuses a top-level number with trailing blanks and numbers longer
  than 63 characters, so such bodies give -102 instead of the documented -104 / -222; the
  harness models this. The state JSON with every value at its device limit and 32-byte SSIDs
  of quotes is 968 bytes (995 with a 32-byte version), 56 bytes below `STATE_SIZE`.
