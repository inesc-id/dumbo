#!/bin/env python3

from common import BenchmarkParameters, CollectData
from parse_sol import Parser
from plot import LinesPlot, BackendDataset

if __name__ == "__main__":
  params = BenchmarkParameters(["-u", "-d", "-i", "-r", "-n"])
  params.set_params("-u", [100])
  params.set_params("-d", [5000000])
  params.set_params("-i", [200000])
  params.set_params("-r", [2000000]) # also set number of buckets to 131072
  params.set_params("-n", [1, 2, 4, 8, 12, 16])
  nb_samples = 10
  locations = [
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/datastructures",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/datastructures"
  ]
  backends = [
    "htm-sgl",
    "htm-sgl-sr"
    # SI-TM --> o custo do Sus-Res está escondido (definir em reunião)
  ]
  data_folder = "dataHTM-SR"

  datasets_thr = {}
  datasets_aborts = {}
  for loc,backend in zip(locations,backends):
    for s in range(nb_samples):
      data = CollectData(
          loc,
          "hashmap/hashmap",
          "build-datastructures.sh",
          backend,
          f"{data_folder}/{backend}-s{s}"
        )
      data.run_sample(params)
      parser = Parser(f"{data_folder}/{backend}-s{s}")
      parser.parse_all(f"{data_folder}/{backend}-s{s}.csv")
    for u in params.params["-u"]:
      if u not in datasets_thr:
        datasets_thr[u] = []
      ds = BackendDataset(
        backend,
        [f"{data_folder}/{backend}-s{s}.csv" for s in range(5)],
        lambda e: e["-n"], "Nb. Threads",
        lambda e: e["-d"]/e["time"], "Throughput (T/s)",
        {"-u": u}
      )
      ds.add_stack("Commits vs Aborts", "Count", {
        "HTM-commits": lambda e: e["htm-commits"],
        "SGL-commits": lambda e: e["gl-commits"],
        "aborts": lambda e: e["total-aborts"]
      })
      ds.add_stack("Abort types", "Nb. aborts", {
        "conflict-transactional": lambda e: e["confl-trans"],
        "conflict-non-transactional": lambda e: e["confl-non-trans"],
        "self": lambda e: e["confl-self"],
        "capacity": lambda e: e["capac-aborts"],
        "persistent": lambda e: e["persis-aborts"],
        "user": lambda e: e["user-aborts"],
        "other": lambda e: e["other-aborts"]
      })
      datasets_thr[u] += [ds]
    
  for k,v in datasets_thr.items():
    lines_plot = LinesPlot(f"{k}% updates", f"thr_{k}upds.pdf")
    lines_plot.plot(v)
    lines_plot.plot_stack(v)
