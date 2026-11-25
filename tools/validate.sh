#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

run_static_checks() (
    python3 tools/check_glyphs.py

    test_dir="$(mktemp -d "${TMPDIR:-/tmp}/zjh-tests.XXXXXX")"
    trap 'rm -rf -- "$test_dir"' EXIT
    sources=(main/zjh_cards.c main/zjh_hand.c main/zjh_round.c main/zjh_ai.c)

    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_zjh_logic.c "${sources[@]}" -o "$test_dir/test_zjh_logic"
    "$test_dir/test_zjh_logic"

    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        -Itests/app_stubs -Icomponents/bsp/include \
        tests/test_zjh_app.c main/zjh_app.c "${sources[@]}" -o "$test_dir/test_zjh_app"
    "$test_dir/test_zjh_app"
)

run_firmware_checks() {
    if ! command -v idf.py >/dev/null 2>&1; then
        printf '%s\n' 'Activate ESP-IDF 5.5.3 before building firmware.' >&2
        return 1
    fi
    idf.py merge-bin -o FoloToy-AI-Passport-full.bin
    python3 tools/verify_firmware.py build
}

case "${1:---all}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static) run_static_checks ;;
    --firmware) run_firmware_checks ;;
    -h|--help) printf '%s\n' "Usage: $0 [--all|--static|--firmware]" ;;
    *) printf '%s\n' "Usage: $0 [--all|--static|--firmware]" >&2; exit 2 ;;
esac
