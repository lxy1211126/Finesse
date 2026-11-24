#!/usr/bin/env python3

import argparse
import csv
import statistics
from collections import defaultdict


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path")
    args = parser.parse_args()
    grouped = defaultdict(list)
    with open(args.csv_path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            grouped[(row["method"], row["dataset"])].append(row)
    print("method,dataset,runs,dcr_mean,dce_residual_fraction_mean,"
          "dce_savings_fraction_mean,sf_speed_mbps_mean,throughput_mbps_mean")
    for (method, dataset), rows in sorted(grouped.items()):
        def mean(field):
            values = [
                float(row[field])
                for row in rows
                if field in row and row[field]
            ]
            return f"{statistics.mean(values):.10g}" if values else ""

        print(",".join((method, dataset, str(len(rows)), mean("dcr"),
                        mean("dce_residual_fraction"),
                        mean("dce_savings_fraction"), mean("sf_speed_mbps"),
                        mean("throughput_mbps"))))


if __name__ == "__main__":
    main()
