# The CI toolchain, locally. `.github/workflows/ci.yml` builds on ubuntu-24.04 with clang++-18 and
# g++-13; this shell pins compilers so `./ci.sh` reproduces every job without pushing. One compiler
# per shell — the compiler is the *stdenv*, not an extra package on PATH, so each leg sees exactly the
# include paths and standard library its CI counterpart does:
#
#     nix-shell --run ./ci.sh                    # clang  (the clang++-18 leg)
#     nix-shell --argstr cc gcc --run ./ci.sh    # gcc 13 (the g++-13 leg)
#     nix-shell                                  # interactive; $CC/$CXX preset
#
# Pinned to nixos-26.05 (597283ad8aa0). Bump the hash to move; no sha256, matching the pinned-tarball
# style used elsewhere — the commit already fixes the contents.
#
# HOW FAITHFUL IS THIS?  On Linux, exactly: clang 18 and gcc 13 are what CI runs. On aarch64-darwin,
# two substitutions are forced, and ci.sh prints the compiler it actually used so a local pass is
# never mistaken for the matrix:
#
#   * clang 18 is unusable there. Its stdenv is not in the binary cache, and building it fails:
#     compiler-rt 18 compiles against the SDK's libc++ 21 headers, which call `__builtin_ctzg` — a
#     builtin clang 18 doesn't have. LLVM 19+ are cached and build fine, so darwin defaults to the
#     nearest working release, 19. Override with `--argstr llvm 20` (or `18` to watch it fail).
#   * gcc is not in the binary cache for aarch64-darwin at all (404 on cache.nixos.org), so
#     `--argstr cc gcc` there means building GCC from source — hours, and gcc-on-darwin is its own
#     adventure. Run the clang leg locally and let CI cover g++-13.
#
# The drift is not pure loss: a newer libc++ than CI's is how you find out that the opt-in
# TACIT_STD_HOLES block trips libc++ 21's `no_specializations` marking on `std::tuple` — real,
# documented-as-ill-formed behaviour that CI's clang 18 is too old to diagnose.
#
# AND ONE THING YOU CANNOT HAVE BOTH OF, on aarch64-darwin only. The `import tacit;` leg needs clang
# and libc++ to be the *same* release; nixpkgs pairs every clang there with the SDK's libc++ 21, and
# every mismatch (clang 19 + libc++ 21, or clang 19 forced onto libc++ 19 headers with -nostdinc++)
# hits libc++'s `__synth_three_way` redeclaration under modules — the darwin twin of the libstdc++
# bug ci.yml pins around with --gcc-install-dir. The only matched pair is `--argstr llvm 21`, but
# libc++ 21 is also the one that enforces `no_specializations`, so the ctest leg fails there instead.
# The default therefore keeps the test suite green and hands ci.sh a reason to skip the module leg:
#
#     nix-shell --run ./ci.sh                    # 15/15 tests + packaging; modules skipped
#     nix-shell --argstr llvm 21 --run ./ci.sh   # modules + packaging; typeapply fails (see above)
{ pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/597283ad8aa0.tar.gz") { }
, cc ? "clang" # "clang" | "gcc"
, llvm ? if pkgs.stdenv.isDarwin then "19" else "18" # see above
, gcc ? "13"
}:

let
  inherit (pkgs) lib;
  llvmPkgs = pkgs."llvmPackages_${llvm}" or (throw "shell.nix: no llvmPackages_${llvm} in nixpkgs");
  gccStdenv = pkgs."gcc${gcc}Stdenv" or (throw "shell.nix: no gcc${gcc}Stdenv in nixpkgs");
  toolchain =
    if cc == "clang" then llvmPkgs.stdenv
    else if cc == "gcc" then gccStdenv
    else throw ''shell.nix: cc must be "clang" or "gcc", got "${cc}"'';
  ciVersion = if cc == "clang" then "18" else "13";
  version = if cc == "clang" then llvm else gcc;
  # See "and one thing you cannot have both of", above.
  mismatchedLibcxx = pkgs.stdenv.isDarwin && cc == "clang" && llvm != "21";
in

(pkgs.mkShell.override { stdenv = toolchain; }) {
  packages = [ pkgs.cmake pkgs.ninja ];

  shellHook = ''
    echo "tacit: $($CXX --version | head -1)"
  '' + lib.optionalString (version != ciVersion) ''
    echo "tacit: note — CI uses ${cc} ${ciVersion}; this shell is ${version} (see shell.nix)."
  '' + lib.optionalString mismatchedLibcxx ''
    export TACIT_SKIP_MODULES="clang ${llvm} against the SDK's libc++ 21; retry with --argstr llvm 21"
  '' + lib.optionalString (cc == "gcc" && pkgs.stdenv.isDarwin) ''
    echo "tacit: warning — gcc is not cached for aarch64-darwin; expect a source build."
  '';
}
