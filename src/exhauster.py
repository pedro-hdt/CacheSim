#!/usr/bin/env python3

# Inf-2C -- Computer Systems
# The University of Edinburgh, 2018
# Bora M. Alper <bora@boramalper.org>

import subprocess
import sys

TF = "mem_trace.txt"


def combinations():
    replacement_policies   = ["FIFO", "LRU", "Random"]
    number_of_cache_blocks = [16, 64, 256, 1024]
    cache_block_size       = [32, 64]
    associativity          = [2**x for x in range(11)]  # 1, 2, 4, 8, ..., 1024

    for rp in replacement_policies:
        for ncb in number_of_cache_blocks:
            for aso in associativity:
                # Associativity cannot be greater than the Number of Cache
                # Blocks!
                if aso > ncb:
                    break

                for cbs in cache_block_size:
                    yield (rp, str(aso), str(ncb), str(cbs), TF)


def main() -> int:
    combs = list(combinations())

    with open("exhaustive-report.txt", "wb") as report:
        for i, comb in enumerate(combs):
            print("\r[%3d/%3d] ./mem_sim %6s %4s %4s %2s %s" % (i+1, len(combs), *comb), end="", flush=True)

            output = subprocess.check_output(
                ["./mem_sim", *comb]
            )
            report.write(output)
            report.write(b"\n\n")

    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
