#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
usage: tools/generate_compile_commands.sh [--board BOARD] [--profile PROFILE]

Generate compile_commands.json for clangd by using the SiFli SDK SCons
compilation database target.

options:
  --board BOARD       SCons board name, defaults to sf32lb52-lchspi-ulp
  --profile PROFILE   SiFli SDK profile, defaults to SIFLI_SDK_PROFILE or default
  -h, --help          show this help and exit
EOF
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
repo_root="$(cd -- "${script_dir}/.." && pwd -P)"
board="sf32lb52-lchspi-ulp"
profile="${SIFLI_SDK_PROFILE:-default}"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --board)
            board="${2:?--board requires a value}"
            shift 2
            ;;
        --board=*)
            board="${1#--board=}"
            shift
            ;;
        --profile)
            profile="${2:?--profile requires a value}"
            shift 2
            ;;
        --profile=*)
            profile="${1#--profile=}"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

sdk_export="${repo_root}/SiFli-SDK/export.sh"
project_dir="${repo_root}/app/project"

if [ ! -f "${sdk_export}" ]; then
    echo "ERROR: missing ${sdk_export}" >&2
    echo "Run: git submodule update --init --recursive SiFli-SDK" >&2
    exit 1
fi

case "${board}" in
    *_hcpu|*_lcpu|*_acpu)
        build_board="${board}"
        ;;
    *)
        build_board="${board}_hcpu"
        ;;
esac

(
    cd "${project_dir}"
    # shellcheck source=/dev/null
    source "${sdk_export}" --profile "${profile}"
    scons --board="${board}" --compiledb cdb
)

compile_db="${project_dir}/build_${build_board}/compile_commands.json"
if [ ! -s "${compile_db}" ]; then
    echo "ERROR: expected compile database was not generated: ${compile_db}" >&2
    exit 1
fi

echo "Generated ${compile_db}"
