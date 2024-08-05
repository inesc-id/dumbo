#!/bin/env python3

from common import BenchmarkParameters, CollectData
from parse_sol import Parser
from plot import LinesPlot, BackendDataset

if __name__ == "__main__":
  params = BenchmarkParameters(["-u", "-d", "-i", "-n"])
  params.set_params("-u", [10, 90])
  params.set_params("-d", [500000])
  params.set_params("-i", [32768])
  params.set_params("-n", [1, 2, 4, 8, 12, 16])
  locations = [
    "/home/ubuntu/PersistentSiHTM/power8tm-pisces/benchmarks/datastructures",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/datastructures",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/datastructures",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/datastructures",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/datastructures",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/datastructures"
  ]
  backends = [
    "pisces",
    "htm-sgl",
    "p8tm-si",
    "spht",
    "tinystm",
    "p8tm-psi-v2-flush"
  ]

  datasets = {}
  for loc,backend in zip(locations,backends):
    for s in range(5):
      data = CollectData(
          loc,
          "hashmap/hashmap",
          "build-datastructures.sh",
          backend,
          f"data/{backend}-s{s}"
        )
      data.run_sample(params)
      parser = Parser(f"data/{backend}-s{s}")
      parser.parse_all(f"data/{backend}-s{s}.csv")
    for u in params.params["-u"]:
      if u not in datasets:
        datasets[u] = []
      datasets[u] += [BackendDataset(backend,
      [f"data/{backend}-s{s}.csv" for s in range(5)],
      lambda e: e["-n"], "Nb. Threads",
      lambda e: e["-d"]/e["time"], "Throughput (T/s)",
      {"-u": u})]
    
  for k,v in datasets.items():
    lines_plot = LinesPlot(f"{k}% updates", f"thr_{k}upds.pdf")
    lines_plot.plot(v)
