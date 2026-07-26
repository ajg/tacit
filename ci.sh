#!/usr/bin/env bash
# Every job in .github/workflows/ci.yml, locally, against one compiler:
#
#     nix-shell --run ./ci.sh                    # clang 18 (the clang++-18 leg)
#     nix-shell --argstr cc gcc --run ./ci.sh    # gcc 13   (the g++-13 leg)
#     CXX=g++-13 ./ci.sh                         # or bring your own compiler, no nix
#
# Jobs, in ci.yml's order: build-and-test (cmake + ctest), modules (`import tacit;`, clang only —
# GCC's module support isn't reliable for this pattern, same as CI), packaging (add_subdirectory,
# then install + find_package). Everything lands in build-ci/, which is disposable.
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")" && pwd)
cd -- "$root"

CXX=${CXX:-c++}
build=${TACIT_BUILD_DIR:-$root/build-ci}
generator=()
command -v ninja >/dev/null 2>&1 && generator=(-G Ninja)

bold=$(printf '\033[1m'); dim=$(printf '\033[2m'); red=$(printf '\033[31m')
green=$(printf '\033[32m'); yellow=$(printf '\033[33m'); off=$(printf '\033[0m')
results=""
job() { printf '\n%s== %s%s\n' "$bold" "$1" "$off"; }
pass() { results="${results}${green}  pass${off}  $1"$'\n'; }
skip() { results="${results}${yellow}  skip${off}  $1 ${dim}($2)${off}"$'\n'; }
fail() { results="${results}${red}  FAIL${off}  $1"$'\n'; }

cxx_version=$("$CXX" --version | head -1)
is_clang=false
case "$cxx_version" in *[Cc]lang*) is_clang=true ;; esac

printf '%stacit ci%s — %s\n%s%s%s\n' "$bold" "$off" "$cxx_version" "$dim" "$("$CXX" -dumpmachine 2>/dev/null || true)" "$off"
rm -rf -- "$build"
mkdir -p -- "$build"

# ---- job: build-and-test -----------------------------------------------------------------------
job "build-and-test"
if cmake -S . -B "$build/main" "${generator[@]}" -DCMAKE_CXX_COMPILER="$CXX" >/dev/null &&
   cmake --build "$build/main" --parallel &&
   ctest --test-dir "$build/main" --output-on-failure; then
  pass "build-and-test"
else
  fail "build-and-test"
fi

# ---- job: modules ------------------------------------------------------------------------------
job "modules (import tacit;)"
if [ -n "${TACIT_SKIP_MODULES:-}" ]; then
  # Set by shell.nix when this platform's clang and libc++ are different releases — libc++ trips its
  # own `__synth_three_way` redeclaration under modules, which says nothing about tacit.
  echo "skipped: $TACIT_SKIP_MODULES"
  skip "modules" "$TACIT_SKIP_MODULES"
elif $is_clang; then
  flags=(-std=c++23 -I"$root/include")
  if "$CXX" "${flags[@]}" --precompile "$root/tacit.cppm" -o "$build/tacit.pcm" &&
     "$CXX" "${flags[@]}" -fmodule-file=tacit="$build/tacit.pcm" \
            "$root/tests/module_check.cpp" "$build/tacit.pcm" -o "$build/module_check" &&
     "$build/module_check"; then
    pass "modules"
  else
    fail "modules"
  fi
else
  echo "skipped: GCC's module support isn't reliable for this pattern (CI runs this on clang only)"
  skip "modules" "clang only"
fi

# ---- job: packaging ----------------------------------------------------------------------------
job "packaging (add_subdirectory)"
if cmake -S tests/packaging -B "$build/pkg" "${generator[@]}" -DCMAKE_CXX_COMPILER="$CXX" >/dev/null &&
   cmake --build "$build/pkg" &&
   ctest --test-dir "$build/pkg" --output-on-failure; then
  pass "packaging: add_subdirectory"
else
  fail "packaging: add_subdirectory"
fi

job "packaging (install + find_package)"
mkdir -p "$build/fp"
cat > "$build/fp/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(fp CXX)
find_package(tacit REQUIRED)
add_executable(a a.cpp)
target_link_libraries(a PRIVATE tacit::tacit)
EOF
cat > "$build/fp/a.cpp" <<'EOF'
#include <tacit/_.hpp>
using tacit::_;
int main() { return (_ == 0)(0) && (0 < _ < 10)(5) ? 0 : 1; }
EOF
if cmake -S . -B "$build/install" "${generator[@]}" -DCMAKE_CXX_COMPILER="$CXX" \
         -DCMAKE_INSTALL_PREFIX="$build/_inst" >/dev/null &&
   cmake --install "$build/install" >/dev/null &&
   cmake -S "$build/fp" -B "$build/fp/build" "${generator[@]}" -DCMAKE_CXX_COMPILER="$CXX" \
         -DCMAKE_PREFIX_PATH="$build/_inst" >/dev/null &&
   cmake --build "$build/fp/build" >/dev/null &&
   "$build/fp/build/a"; then
  pass "packaging: install + find_package"
else
  fail "packaging: install + find_package"
fi

# ---- summary -----------------------------------------------------------------------------------
printf '\n%ssummary%s (%s)\n%s' "$bold" "$off" "$cxx_version" "$results"
case "$results" in *FAIL*) exit 1 ;; esac
