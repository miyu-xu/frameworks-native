#!/bin/bash
# Check that the built libbinder-rpc exports all expected NDK symbols.
# Usage: check_symbols.sh <path-to-libbinder-rpc.so|dylib>

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <path-to-libbinder-rpc>"
    exit 1
fi

LIB="$1"
if [[ ! -f "$LIB" ]]; then
    echo "ERROR: library not found: $LIB"
    exit 1
fi

# Expected symbols — the actual NDK Binder RPC API exported by this library.
EXPECTED=(
    # RPC Server API
    ARpcServer_newVsock
    ARpcServer_newBoundSocket
    ARpcServer_newUnixDomainBootstrap
    ARpcServer_newInet
    ARpcServer_setSupportedFileDescriptorTransportModes
    ARpcServer_setMaxThreads
    ARpcServer_join
    ARpcServer_shutdown
    ARpcServer_start

    # RPC Session API
    ARpcSession_new
    ARpcSession_setMaxIncomingThreads
    ARpcSession_setMaxOutgoingConnections
    ARpcSession_setupUnixDomainBootstrapClient
    ARpcSession_setupUnixDomainClient
    ARpcSession_setupVsockClient
    ARpcSession_setupInet
    ARpcSession_setupPreconnectedClient
    ARpcSession_setFileDescriptorTransportMode

    # NDK Binder API
    AIBinder_new
    AIBinder_transact
    AIBinder_ping
    AIBinder_dump
    AIBinder_linkToDeath
    AIBinder_unlinkToDeath
    AIBinder_isAlive
    AIBinder_isRemote
    AIBinder_setRequestingSid
    AIBinder_associateClass
    AIBinder_Class_define
    AIBinder_Class_setOnDump
    AIBinder_Class_getDescriptor

    # Ref-counting
    AIBinder_incStrong
    AIBinder_decStrong
    AIBinder_Weak_new
    AIBinder_Weak_promote
    AIBinder_Weak_delete
)

MISSING=0
case "$(uname -s)" in
    Darwin)
        # macOS nm prefixes symbols with underscore; strip it for matching.
        SYMBOLS=$(nm -g "$LIB" 2>/dev/null | grep ' T ' | awk '{print $3}' | sed 's/^_//' || true)
        ;;
    Linux)
        SYMBOLS=$(nm -D "$LIB" 2>/dev/null | grep ' T ' | awk '{print $3}' || true)
        ;;
    *)
        if command -v dumpbin >/dev/null 2>&1; then
            SYMBOLS=$(dumpbin /EXPORTS "$LIB" | tail -n +20 | awk '{print $NF}' | grep -i '^AIBinder\|^ARpc' || true)
        else
            SYMBOLS=$(nm -g "$LIB" 2>/dev/null | awk '{print $3}' || true)
        fi
        ;;
esac

echo "Checking symbols in: $LIB"
echo ""

for sym in "${EXPECTED[@]}"; do
    if grep -q "$sym" <<< "$SYMBOLS"; then
        echo "  [OK]   $sym"
    else
        echo "  [MISS] $sym"
        MISSING=$((MISSING + 1))
    fi
done

if [[ $MISSING -gt 0 ]]; then
    echo ""
    echo "FAIL: $MISSING symbol(s) missing"
    exit 1
else
    echo ""
    echo "PASS: all ${#EXPECTED[@]} expected symbols found"
    exit 0
fi
