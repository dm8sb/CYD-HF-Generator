## Summary

<!-- What changes and why, in a few sentences. Name the finding IDs (C-n, S-n, U-n, P-n, T-n, W-S…) this addresses. -->

**Step:** <!-- step ID from ARBEITSPLAN.md section 4, e.g. S55 or G7; "none" for outside contributions -->

## Gate (ARBEITSPLAN.md section 0)

One step = one commit; the gate holds before the commit. Tick what applies, strike through (`~~…~~`) what does not and say why.

- [ ] Host tests green: `native` and `native-fault` (Windows: `native-win`, `native-win-fault`)
- [ ] 0 warnings in `src/` (`-Wall -Wextra`), both firmware envs
- [ ] New logic has a host test, and the test fails without the fix (counter-check)
- [ ] User manual DE **and** EN updated in this commit (DE first, EN is its translation)
- [ ] PROJECT_SPECIFICATION.md (requirements A-n / decisions E-n) updated in this commit
- [ ] README.md updated in this commit (changes per version, pinout, build)
- [ ] ARBEITSPLAN.md updated and the step ticked in this commit
- [ ] UI change: flashed and checked on the device (board: original CYD / CYD2USB) before the commit
- [ ] Network code: reviewed against the security requirements W-S (ARBEITSPLAN.md section 3.7)
- [ ] No credentials in the diff (`include/test_wifi_local.h` stays untracked)

## Firmware size

<!-- From the end of `pio run` ("RAM: … used N bytes", "Flash: … used N bytes"). -->

| Env | Flash (bytes, %) | RAM (bytes, %) | Change vs. target branch |
|---|---:|---:|---:|
| `esp32-2432S028R` | | | |

## Test notes

<!-- How it was checked: host test suites, device checks (board, steps), measurements (MESSPLAN IDs). -->
