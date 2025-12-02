# KV Cumulative CRC Verification Script

This script verifies that all Raft peers in a cluster have matching 
KV cumulative CRCs by SSHing to each peer and querying their 
RocksDB databases.

## Script: `verify_kv_crc.sh`

Simple bash script for verifying KV CRC consistency across all 
peers in a cluster.

**Features:**
- Parse peer configuration files (`.peer`)y
- SSH to each peer using IP addresses from config
- Query RocksDB databases via `ldb` on remote hosts
- Parse `struct raft_last_applied` binary data
- Compare KV cumulative CRCs across all peers
- Colorized output with clear success/failure indicators

**Requirements:**
- Bash 4.0+
- SSH access to all peers (passwordless SSH keys recommended)
- `ldb` tool installed on all peer hosts
- Python 3 (for binary struct parsing)

**Usage:**
```bash
./verify_kv_crc.sh <config_dir>

# Example:
./verify_kv_crc.sh /path/to/holon_output/<raft-uuid>/configs
```

## Configuration Format

The scripts expect a directory containing:
- One `<raft-uuid>.raft` file (optional)
- Multiple `<peer-uuid>.peer` files

### Peer Configuration File Format (`.peer`)
```
RAFT         <raft-uuid>
IPADDR       <ip-address>
PORT         <port>
CLIENT_PORT  <client-port>
STORE        <path-to-raft-db>
```

Example:
```
RAFT         ae163060-a8dd-11f0-9db9-27246877d5d4
IPADDR       127.0.0.1
PORT         8001
CLIENT_PORT  9001
STORE        /home/user/holon_output/raftdb/ae6a26f2-a8dd-11f0-b092-0b6a3052d2b7.raftdb
```

## Database Structure

The scripts query the `a1_hdr.last_applied` key from RocksDB, which contains:

```c
struct raft_last_applied {
    int64_t  rla_idx;              // Last applied log index
    uint32_t rla_sub_idx;          // Sub-entry index
    uint32_t rla_sub_idx_max;      // Max sub-entry index
    int64_t  rla_synced_idx;       // Last synced index
    uint32_t rla_cumulative_crc;   // Entry header CRC
    uint32_t rla_kv_cumulative_crc;// KV cumulative CRC ← VERIFIED
};
```

## Output Examples

### Success (All Peers Match)
```
================================================================================
KV Cumulative CRC Verification
================================================================================

Peer: ae6a26f2... (127.0.0.1:8001)
  ✓ LAI=3, Entry_CRC=0x0f10fc6a, KV_CRC=0xa94148c2

Peer: ae6ac62a... (127.0.0.1:8002)
  ✓ LAI=3, Entry_CRC=0x0f10fc6a, KV_CRC=0xa94148c2

...

================================================================================
Verification Results
================================================================================

✅ SUCCESS: All 5 peers have matching KV CRC: 0xa94148c2
✅ All peers at same index: 3
```

### Failure (Mismatch Detected)
```
================================================================================
Verification Results
================================================================================

❌ FAILURE: KV CRC mismatch detected!

   Peer breakdown:
     ae6a26f2... : 0xa94148c2 at index 3
     ae6ac62a... : 0xdeadbeef at index 3
     ...
```

## Exit Codes

- `0`: Success - all peers have matching KV CRCs
- `1`: Failure - mismatch detected or query error

## Manual Verification

You can also manually query a single peer via SSH:

```bash
# Query the database on remote host
ssh <peer-ip> "ldb --db=/path/to/raftdb/db get \
  'a1_hdr.last_applied' --value_hex"

# Parse with Python
python3 << 'EOF'
import struct
hex_str = "0300000000000000010000000100000002000000000000006AFC100FC24841A9"
data = bytes.fromhex(hex_str)
rla_idx, sub_idx, sub_idx_max, synced_idx, cum_crc = \
  struct.unpack('<QIIQi', data[:28])
kv_crc = struct.unpack('<I', data[28:32])[0] if len(data) >= 32 \
  else None
print(f"Last Applied Index: {rla_idx}")
print(f"KV Cumulative CRC: 0x{kv_crc:08x}")
EOF
```

## SSH Configuration

For passwordless operation, set up SSH keys:

```bash
# Generate SSH key (if not already done)
ssh-keygen -t ed25519

# Copy to all peers
ssh-copy-id user@<peer-ip-1>
ssh-copy-id user@<peer-ip-2>
# ... for all peers
```

