#!/usr/bin/env python3
"""
Generate ClassBench-format rule files and matching packet trace files
for testing HybridTSS.

Usage:
    uv run scripts/gen_testdata.py [--rules N] [--packets M] [--out-dir DIR] [--seed S]
    # fallback:
    python3 scripts/gen_testdata.py [--rules N] [--packets M] [--out-dir DIR] [--seed S]

Outputs:
    <out-dir>/test_<N>        - rule file
    <out-dir>/test_<N>_trace  - packet trace file
"""

import argparse
import os
import random
import struct


def ip_to_octets(ip_int):
    """Convert a 32-bit integer to 4 octets."""
    return (
        (ip_int >> 24) & 0xFF,
        (ip_int >> 16) & 0xFF,
        (ip_int >> 8) & 0xFF,
        ip_int & 0xFF,
    )


def generate_prefix(rng):
    """Generate a random IP prefix (base_ip, mask_len) with 0-32 bit mask."""
    mask_len = rng.choice([0, 8, 16, 24, 32, rng.randint(1, 32)])
    if mask_len == 0:
        return 0, 0
    # Generate a random base IP and zero out host bits
    base = rng.getrandbits(32)
    if mask_len < 32:
        base = (base >> (32 - mask_len)) << (32 - mask_len)
    return base & 0xFFFFFFFF, mask_len


def prefix_range(base, mask_len):
    """Return (low, high) for a prefix."""
    if mask_len == 0:
        return 0, 0xFFFFFFFF
    low = base
    high = base + (1 << (32 - mask_len)) - 1
    return low & 0xFFFFFFFF, high & 0xFFFFFFFF


def generate_port_range(rng):
    """Generate a random port range. Biased toward common patterns."""
    kind = rng.choice(["any", "exact", "range", "prefix"])
    if kind == "any":
        return 0, 65535
    elif kind == "exact":
        p = rng.randint(0, 65535)
        return p, p
    elif kind == "range":
        lo = rng.randint(0, 65000)
        hi = rng.randint(lo, min(lo + 1024, 65535))
        return lo, hi
    else:  # prefix-aligned range
        bits = rng.randint(0, 16)
        lo = rng.randint(0, 65535) & (0xFFFF << (16 - bits)) & 0xFFFF
        hi = lo | ((1 << (16 - bits)) - 1)
        return lo, min(hi, 65535)


def generate_protocol(rng):
    """Generate protocol and mask. Common: TCP(6), UDP(17), any(0/0)."""
    kind = rng.choice(["tcp", "udp", "any", "any", "any"])
    if kind == "tcp":
        return 6, 0xFF
    elif kind == "udp":
        return 17, 0xFF
    else:
        return 0, 0


def generate_rules(num_rules, rng):
    """Generate a list of rules."""
    rules = []
    for _ in range(num_rules):
        sip, smask = generate_prefix(rng)
        dip, dmask = generate_prefix(rng)
        sport_lo, sport_hi = generate_port_range(rng)
        dport_lo, dport_hi = generate_port_range(rng)
        proto, proto_mask = generate_protocol(rng)
        rules.append(
            {
                "sip": sip,
                "smask": smask,
                "dip": dip,
                "dmask": dmask,
                "sport_lo": sport_lo,
                "sport_hi": sport_hi,
                "dport_lo": dport_lo,
                "dport_hi": dport_hi,
                "proto": proto,
                "proto_mask": proto_mask,
            }
        )
    return rules


def rule_matches_packet(rule, sip, dip, sport, dport, proto):
    """Check if a packet matches a rule."""
    # Source IP
    s_lo, s_hi = prefix_range(rule["sip"], rule["smask"])
    if sip < s_lo or sip > s_hi:
        return False
    # Dest IP
    d_lo, d_hi = prefix_range(rule["dip"], rule["dmask"])
    if dip < d_lo or dip > d_hi:
        return False
    # Source port
    if sport < rule["sport_lo"] or sport > rule["sport_hi"]:
        return False
    # Dest port
    if dport < rule["dport_lo"] or dport > rule["dport_hi"]:
        return False
    # Protocol
    if rule["proto_mask"] == 0xFF:
        if proto != rule["proto"]:
            return False
    # proto_mask == 0 means wildcard, always matches
    return True


def find_best_match(rules, sip, dip, sport, dport, proto):
    """Find the index of the highest-priority matching rule.
    Priority: rule 0 has highest priority (priority = num_rules - 1 - index).
    So the first matching rule by index is the best match."""
    for i, rule in enumerate(rules):
        if rule_matches_packet(rule, sip, dip, sport, dport, proto):
            return i
    return len(rules)  # no match sentinel


def generate_packet_for_rule(rule, rng):
    """Generate a packet that definitely matches the given rule."""
    # Source IP: random within prefix range
    s_lo, s_hi = prefix_range(rule["sip"], rule["smask"])
    sip = rng.randint(s_lo, s_hi)
    # Dest IP
    d_lo, d_hi = prefix_range(rule["dip"], rule["dmask"])
    dip = rng.randint(d_lo, d_hi)
    # Ports
    sport = rng.randint(rule["sport_lo"], rule["sport_hi"])
    dport = rng.randint(rule["dport_lo"], rule["dport_hi"])
    # Protocol
    if rule["proto_mask"] == 0xFF:
        proto = rule["proto"]
    else:
        proto = rng.choice([6, 17, 1, 0])
    return sip, dip, sport, dport, proto


def generate_random_packet(rng):
    """Generate a fully random packet (may not match any rule)."""
    sip = rng.getrandbits(32)
    dip = rng.getrandbits(32)
    sport = rng.randint(0, 65535)
    dport = rng.randint(0, 65535)
    proto = rng.choice([6, 17, 1, 0])
    return sip, dip, sport, dport, proto


def write_rules(rules, filepath):
    """Write rules in ClassBench format."""
    with open(filepath, "w") as f:
        for rule in rules:
            s = ip_to_octets(rule["sip"])
            d = ip_to_octets(rule["dip"])
            f.write(
                f"@{s[0]}.{s[1]}.{s[2]}.{s[3]}/{rule['smask']}\t"
                f"{d[0]}.{d[1]}.{d[2]}.{d[3]}/{rule['dmask']}\t"
                f"{rule['sport_lo']} : {rule['sport_hi']}\t"
                f"{rule['dport_lo']} : {rule['dport_hi']}\t"
                f"{rule['proto']:02x}/{rule['proto_mask']:02x}\t"
                f"0000/0000\n"
            )


def write_packets(packets, filepath):
    """Write packets in trace format: src_ip dst_ip sport dport proto proto_mask fid"""
    with open(filepath, "w") as f:
        for sip, dip, sport, dport, proto, proto_mask, fid in packets:
            f.write(f"{sip} {dip} {sport} {dport} {proto} {proto_mask} {fid}\n")


def main():
    parser = argparse.ArgumentParser(description="Generate test data for HybridTSS")
    parser.add_argument(
        "--rules", type=int, default=100, help="Number of rules (default: 100)"
    )
    parser.add_argument(
        "--packets", type=int, default=1000, help="Number of packets (default: 1000)"
    )
    parser.add_argument(
        "--out-dir", type=str, default="Data", help="Output directory (default: Data)"
    )
    parser.add_argument(
        "--seed", type=int, default=42, help="Random seed (default: 42)"
    )
    args = parser.parse_args()

    rng = random.Random(args.seed)
    os.makedirs(args.out_dir, exist_ok=True)

    print(f"Generating {args.rules} rules...")
    rules = generate_rules(args.rules, rng)

    # Generate packets: ~70% targeted (match a specific rule), ~30% random
    print(f"Generating {args.packets} packets...")
    packets = []
    n_targeted = int(args.packets * 0.7)
    n_random = args.packets - n_targeted

    for _ in range(n_targeted):
        rule_idx = rng.randint(0, len(rules) - 1)
        sip, dip, sport, dport, proto = generate_packet_for_rule(rules[rule_idx], rng)
        fid = find_best_match(rules, sip, dip, sport, dport, proto)
        proto_mask = 0xFF if rules[fid]["proto_mask"] == 0xFF else 0
        packets.append((sip, dip, sport, dport, proto, proto_mask, fid))

    for _ in range(n_random):
        sip, dip, sport, dport, proto = generate_random_packet(rng)
        fid = find_best_match(rules, sip, dip, sport, dport, proto)
        if fid < len(rules):
            proto_mask = 0xFF if rules[fid]["proto_mask"] == 0xFF else 0
        else:
            proto_mask = 0
        packets.append((sip, dip, sport, dport, proto, proto_mask, fid))

    # Shuffle packets
    rng.shuffle(packets)

    # Write output
    rule_file = os.path.join(args.out_dir, f"test_{args.rules}")
    trace_file = os.path.join(args.out_dir, f"test_{args.rules}_trace")

    write_rules(rules, rule_file)
    write_packets(packets, trace_file)

    # Stats
    matched = sum(1 for p in packets if p[6] < len(rules))
    print(f"Done!")
    print(f"  Rules:   {rule_file}  ({args.rules} rules)")
    print(f"  Packets: {trace_file}  ({args.packets} packets, {matched} matched)")


if __name__ == "__main__":
    main()
