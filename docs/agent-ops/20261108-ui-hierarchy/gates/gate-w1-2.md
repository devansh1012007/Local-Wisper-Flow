# Gate — wave 1 run 2 (post-improvement) — 2026-11-08
- Tree: git status --porcelain → M COMMS.md only (orchestrator + improver appends, pending commit)
- HEAD: dd90b17 (after W1-IMPR-01 1f7a71d + W1-IMPR-02 bbc8a14)
- Wave 1 base: 2c06061

| Step | Command | Result | vs gate w1-1 |
| Build lwf_tests | cmake --build build --target lwf_tests --parallel 4 | Built | match |
| Build full exe | cmake --build build --target local-wisper-flow --parallel 4 | Built target local-wisper-flow | match (gate-w1-1 also Built) |
| Unit ctest | ctest --test-dir build -R lwf-tests | 1/1 Passed 19.97s | match (was 19.76s) |
| Hygiene | bash scripts/check_hygiene.sh | hygiene: OK | match |
| No-network | bash scripts/check_no_network.sh | no-network: OK | match |
| G3 zig | not run | pending (no zig) | pending |
| New scanner negative test | ctest green includes new TEST_CASE (6/6 in W1-IMPR-02 report) | green | improvement verified |

New failures: none. Improvements: R-W1-001 code + R-W1-001 test FIXED. Delta review pending. Screenshots P-006 still pending manual Windows.
