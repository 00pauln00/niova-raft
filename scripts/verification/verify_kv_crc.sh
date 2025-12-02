#!/bin/bash
#
# KV Cumulative CRC Verification Script (Bash version)
#
# This script verifies that all Raft peers in a cluster have matching KV cumulative CRCs.
# It reads peer configurations and checks their RocksDB databases locally.
#
# Usage:
#   ./verify_kv_crc.sh <config_dir>
#   ./verify_kv_crc.sh /path/to/holon_output/<raft-uuid>/configs
#

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse command line
if [ $# -ne 1 ]; then
    echo "Usage: $0 <config_dir>" >&2
    echo "Example: $0 /path/to/holon_output/<raft-uuid>/configs" >&2
    exit 1
fi

CONFIG_DIR="$1"

if [ ! -d "$CONFIG_DIR" ]; then
    echo "Error: $CONFIG_DIR is not a directory" >&2
    exit 1
fi

# Find all peer configs
PEER_CONFIGS=$(find "$CONFIG_DIR" -name "*.peer" | sort)

if [ -z "$PEER_CONFIGS" ]; then
    echo "Error: No peer configurations found in $CONFIG_DIR" >&2
    exit 1
fi

PEER_COUNT=$(echo "$PEER_CONFIGS" | wc -l)
echo "Found $PEER_COUNT peers in cluster"
echo

echo "================================================================================"
echo "KV Cumulative CRC Verification"
echo "================================================================================"
echo

# Function to parse last_applied structure
parse_last_applied() {
    local hex_value="$1"
    
    python3 << EOF
import struct
import sys

hex_str = "$hex_value".replace("0x", "")
data = bytes.fromhex(hex_str)

if len(data) < 28:
    print("ERROR: Invalid data length", file=sys.stderr)
    sys.exit(1)

try:
    rla_idx, sub_idx, sub_idx_max, synced_idx, cum_crc = struct.unpack('<QIIQi', data[:28])
    
    kv_crc = None
    if len(data) >= 32:
        kv_crc = struct.unpack('<I', data[28:32])[0]
    
    print(f"{rla_idx}")
    print(f"{cum_crc & 0xFFFFFFFF:08x}")
    if kv_crc is not None:
        print(f"{kv_crc:08x}")
    else:
        print("MISSING")
except Exception as e:
    print(f"ERROR: {e}", file=sys.stderr)
    sys.exit(1)
EOF
}

# Query each peer
declare -A KV_CRCS
declare -A INDICES
SUCCESS=true

for peer_config in $PEER_CONFIGS; do
    peer_uuid=$(basename "$peer_config" .peer)
    
    # Parse peer config
    store=$(grep "^STORE" "$peer_config" | awk '{print $2}')
    ipaddr=$(grep "^IPADDR" "$peer_config" | awk '{print $2}')
    port=$(grep "^PORT" "$peer_config" | awk '{print $2}')
    
    db_path="$store/db"
    
    echo "Peer: ${peer_uuid:0:8}... ($ipaddr:$port)"
    
    # Query ldb
    hex_value=$(ldb --db="$db_path" get "a1_hdr.last_applied" --value_hex 2>/dev/null || echo "")
    
    if [ -z "$hex_value" ]; then
        echo -e "  ${RED}❌ Failed to query peer${NC}"
        echo
        SUCCESS=false
        continue
    fi
    
    # Parse the structure
    result=$(parse_last_applied "$hex_value" 2>&1) || {
        echo -e "  ${RED}❌ Failed to parse data${NC}"
        echo
        SUCCESS=false
        continue
    }
    
    rla_idx=$(echo "$result" | sed -n '1p')
    cum_crc=$(echo "$result" | sed -n '2p')
    kv_crc=$(echo "$result" | sed -n '3p')
    
    if [ "$kv_crc" = "MISSING" ]; then
        echo -e "  ${RED}❌ KV cumulative CRC not present${NC}"
        echo
        SUCCESS=false
        continue
    fi
    
    KV_CRCS[$peer_uuid]=$kv_crc
    INDICES[$peer_uuid]=$rla_idx
    
    echo "  ✓ LAI=$rla_idx, Entry_CRC=0x$cum_crc, KV_CRC=0x$kv_crc"
    echo
done

echo "================================================================================"
echo "Verification Results"
echo "================================================================================"
echo

if [ ${#KV_CRCS[@]} -eq 0 ]; then
    echo -e "${RED}❌ No peers could be queried!${NC}"
    exit 1
fi

# Check if all KV CRCs match
UNIQUE_CRCS=$(printf '%s\n' "${KV_CRCS[@]}" | sort -u | wc -l)

if [ "$UNIQUE_CRCS" -eq 1 ]; then
    # Get the single CRC value
    KV_CRC_VALUE=$(printf '%s\n' "${KV_CRCS[@]}" | head -1)
    echo -e "${GREEN}✅ SUCCESS: All ${#KV_CRCS[@]} peers have matching KV CRC: 0x$KV_CRC_VALUE${NC}"
    
    # Check indices
    UNIQUE_INDICES=$(printf '%s\n' "${INDICES[@]}" | sort -u | wc -l)
    if [ "$UNIQUE_INDICES" -eq 1 ]; then
        INDEX_VALUE=$(printf '%s\n' "${INDICES[@]}" | head -1)
        echo -e "${GREEN}✅ All peers at same index: $INDEX_VALUE${NC}"
    else
        echo -e "${YELLOW}⚠️  Peers at different indices${NC}"
        echo "   (This is OK if cluster is still applying)"
    fi
    
    exit 0
else
    echo -e "${RED}❌ FAILURE: KV CRC mismatch detected!${NC}"
    echo
    echo "   Peer breakdown:"
    for peer_uuid in "${!KV_CRCS[@]}"; do
        echo "     ${peer_uuid:0:8}... : 0x${KV_CRCS[$peer_uuid]} at index ${INDICES[$peer_uuid]}"
    done
    
    exit 1
fi


