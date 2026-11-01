#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cache="$root/third_party/cache"
prefix="$root/third_party/prefix"
jobs="$(nproc 2>/dev/null || echo 2)"
offline=0
only=""
with_xen=0

usage() {
    cat <<'EOF'
usage: bootstrap-third-party.sh [--offline] [--only DEP] [--prefix DIR]
                                [--cache DIR] [--jobs N] [--with-xen]
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --offline) offline=1; shift ;;
        --only) only="${2:?}"; shift 2 ;;
        --prefix) prefix="${2:?}"; shift 2 ;;
        --cache) cache="${2:?}"; shift 2 ;;
        --jobs) jobs="${2:?}"; shift 2 ;;
        --with-xen) with_xen=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -n "$only" ]]; then
    deps=("$only")
else
    deps=(json sodium pg qt6)
    if [[ "$with_xen" -eq 1 ]]; then
        deps+=(xen)
    fi
fi

mkdir -p "$cache" "$prefix" "$root/third_party/build"

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        echo "no SHA-256 tool found" >&2
        return 1
    fi
}

info_for() {
    cmake -DNAME="$1" -P "$root/scripts/third-party-info.cmake" 2>&1
}

read_info() {
    local dep="$1" out
    out="$(info_for "$dep")"
    VERSION="$(sed -n 's/^VERSION=//p' <<<"$out")"
    URL="$(sed -n 's/^URL=//p' <<<"$out")"
    SHA256="$(sed -n 's/^SHA256=//p' <<<"$out")"
    PATCHES="$(sed -n 's/^PATCHES=//p' <<<"$out")"
    if [[ -z "$VERSION" || -z "$URL" || -z "$SHA256" ]]; then
        echo "unknown dependency: $dep" >&2
        exit 1
    fi
}

build_json() {
    local src="$1"
    cmake -S "$src" -B "$src/build" \
        -DJSON_BuildTests=OFF \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$src/build" -j"$jobs"
    cmake --install "$src/build"
}

build_sodium() {
    local src="$1"
    (
        cd "$src"
        ./configure --prefix="$prefix" --disable-dependency-tracking
        make -j"$jobs"
        make install
    )
}

build_pg() {
    local src="$1"
    (
        cd "$src"
        ./configure --prefix="$prefix" --without-readline --without-zlib
        make -j"$jobs"
        make install
    )
}

build_qt6() {
    local src="$1"
    (
        cd "$src"
        ./configure -prefix "$prefix" -opensource -confirm-license -release \
            -nomake examples -nomake tests \
            -skip qtwebengine -skip qtwebview -skip qt3d -skip qtcharts \
            -skip qtdatavis3d -skip qtquick3d -skip qtmultimedia \
            -skip qtspeech -skip qtvirtualkeyboard -skip qtnetworkauth \
            -skip qtdoc
        cmake --build . -j"$jobs"
        cmake --install .
    )
}

build_qt5() {
    local src="$1"
    (
        cd "$src"
        ./configure -prefix "$prefix" -opensource -confirm-license -release \
            -nomake examples -nomake tests \
            -skip qtwebengine -skip qtwebview -skip qt3d -skip qtcharts \
            -skip qtdatavis3d -skip qtquick3d -skip qtmultimedia \
            -skip qtspeech -skip qtvirtualkeyboard -skip qtnetworkauth \
            -skip qtdoc
        make -j"$jobs"
        make install
    )
}

build_xen() {
    local _src="$1"
    if [[ "${OYAMA_XEN_BUILD:-0}" != "1" ]]; then
        echo "xen build is implemented in Phase D" >&2
        exit 1
    fi
    echo "xen build is implemented in Phase D" >&2
    exit 1
}

apply_patches() {
    local src="$1" patches="$2"
    [[ -z "$patches" ]] && return 0

    local patch_path="$root/third_party/$patches"
    if [[ ! -e "$patch_path" ]]; then
        echo "patch path missing: $patch_path" >&2
        exit 1
    fi

    if [[ -d "$patch_path" ]]; then
        local p
        for p in "$patch_path"/*.patch; do
            [[ -f "$p" ]] || continue
            (cd "$src" && patch -p1 --forward <"$p")
        done
    elif [[ "$patch_path" == *.tar.gz || "$patch_path" == *.tar.xz || "$patch_path" == *.tgz ]]; then
        local tmp p
        tmp="$(mktemp -d)"
        tar -xf "$patch_path" -C "$tmp"
        for p in "$tmp"/*.patch "$tmp"/*/*.patch; do
            [[ -f "$p" ]] || continue
            (cd "$src" && patch -p1 --forward <"$p")
        done
        rm -rf "$tmp"
    else
        (cd "$src" && patch -p1 --forward <"$patch_path")
    fi
}

for dep in "${deps[@]}"; do
    read_info "$dep"
    stamp="$prefix/.stamp-$dep"
    if [[ -f "$stamp" ]] && [[ "$(cat "$stamp")" == "$dep $VERSION $SHA256" ]]; then
        echo "skip: $dep $VERSION (stamp matches)"
        continue
    fi

    file="$cache/$(basename "$URL")"
    if [[ ! -f "$file" ]]; then
        if [[ "$offline" -eq 1 ]]; then
            echo "offline: missing $file; populate third_party/cache" >&2
            exit 1
        fi
        echo "download: $URL"
        curl -fL --retry 3 --connect-timeout 20 -o "$file" "$URL"
    fi

    actual="$(sha256_of "$file")"
    if [[ "$actual" != "$SHA256" ]]; then
        echo "sha256 mismatch: $file" >&2
        echo "  expected $SHA256" >&2
        echo "  actual   $actual" >&2
        exit 1
    fi

    src="$root/third_party/build/$dep-$VERSION"
    rm -rf "$src"
    mkdir -p "$src"
    case "$file" in
        *.tar.gz | *.tgz) tar -xzf "$file" -C "$src" --strip-components=1 ;;
        *.tar.xz) tar -xJf "$file" -C "$src" --strip-components=1 ;;
        *.tar.bz2) tar -xjf "$file" -C "$src" --strip-components=1 ;;
        *) echo "unsupported archive: $file" >&2; exit 1 ;;
    esac

    apply_patches "$src" "$PATCHES"

    case "$dep" in
        json) build_json "$src" ;;
        sodium) build_sodium "$src" ;;
        pg) build_pg "$src" ;;
        qt6) build_qt6 "$src" ;;
        qt5) build_qt5 "$src" ;;
        xen) build_xen "$src" ;;
        *) echo "unknown dependency: $dep" >&2; exit 1 ;;
    esac

    printf '%s %s %s\n' "$dep" "$VERSION" "$SHA256" >"$stamp"
    echo "installed: $dep $VERSION"
done

{
    echo "oyama third-party prefix"
    for dep in "${deps[@]}"; do
        read_info "$dep"
        printf '%s %s\n' "$dep" "$VERSION"
    done
} >"$prefix/.stamp"
echo "prefix stamp: $prefix/.stamp"
