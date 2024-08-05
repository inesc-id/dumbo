#!/bin/env python3

from common import BenchmarkParameters, CollectData
from parse_sol import Parser
from plot import LinesPlot, BackendDataset

if __name__ == "__main__":
  params = BenchmarkParameters(["-n", "-m"])
  # params = BenchmarkParameters(["-u", "-d", "-i", "-r", "-n"])
  # params.set_params("-u", [100])
  # params.set_params("-d", [5000000])
  # params.set_params("-i", [262144])
  # params.set_params("-r", [262144])
  params.set_params("-n", [1, 2, 4, 8, 12, 16, 20, 24, 32])
  params.set_params("-m", [2, 16])
  nb_samples = 3
  locations = [
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/array",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/array",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/array",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/array",
    "/home/ubuntu/PersistentSiHTM/POWER8TM/benchmarks/array"
  ]
  backends = [
    "p8tm-psi-v2-fi-improved",
    "p8tm-psi-v2-ci",
    "p8tm-psi-v2-fi-improved-lc-hidden",
    "p8tm-psi-v2-ci-lc-hidden"
  ]
  name_map = {
    "p8tm-psi-v2-fi-improved" : "PSI-OL",
    "p8tm-psi-v2-ci" : "PSI",
    "p8tm-psi-v2-fi-improved-lc-hidden" : "PSI-OL (LC)",
    "p8tm-psi-v2-ci-lc-hidden" : "PSI (LC)"
  }
  data_folder = "dataLogicalClock" # erase the folder on re-run

  datasets_thr = {}
  datasets_aborts = {}
  for loc,backend in zip(locations,backends):
    for s in range(nb_samples):
      data = CollectData(
          loc,
          "array/array",
          "build-array.sh",
          backend,
          f"{data_folder}/{backend}-s{s}"
        )
      # data.run_sample(params)
      parser = Parser(f"{data_folder}/{backend}-s{s}")
      parser.parse_all(f"{data_folder}/{backend}-s{s}.csv")
    for i in params.params["-m"]: # TODO: this for-loop was originally to plot 10%, 50%, 90% updates
      if i not in datasets_thr:
        datasets_thr[i] = []
      ds = BackendDataset(
        name_map[backend],
        [f"{data_folder}/{backend}-s{s}.csv" for s in range(nb_samples)],
        lambda e: e["-n"], "Nb. Threads",
        lambda e: e["total-commits"]/e["time"], "Throughput (T/s)", {"-m": i}
      )
      ds.add_stack("Commits vs Aborts", "Count", {
        "HTM-commits": lambda e: e["htm-commits"],
        "ROT-commits": lambda e: e["rot-commits"],
        "SGL-commits": lambda e: e["gl-commits"],
        "aborts": lambda e: e["total-aborts"]
      })
      ds.add_stack("Abort types", "Nb. aborts", {
        "conflict-transactional": lambda e: e["rot-trans-aborts"],
        "conflict-non-transactional": lambda e: e["rot-non-trans-aborts"],
        "self": lambda e: e["rot-self-aborts"],
        "capacity": lambda e: e["rot-capac-aborts"],
        "persistent": lambda e: e["rot-persis-aborts"],
        "user": lambda e: e["rot-user-aborts"],
        "other": lambda e: e["rot-other-aborts"]
      })
      ds.add_stack("Time profiling", "Ratio of time", {
        "time-commit": lambda e: e["total-commit-time"] / e["-n"] / e["total-sum-time"],
        "time-abort": lambda e: e["total-abort-time"] / e["-n"] / e["total-sum-time"],
        "time-wait": lambda e: e["total-wait-time"] / e["-n"] / e["total-sum-time"],
        "time-sus": lambda e: e["total-sus-time"] / e["-n"] / e["total-sum-time"],
        "time-flush": lambda e: e["total-flush-time"] / e["-n"] / e["total-sum-time"]
      })
      datasets_thr[i] += [ds]
    
  for k,v in datasets_thr.items():
    lines_plot = LinesPlot(f"Nb Writes = {k}", f"thr_{k}writes.pdf")
    lines_plot.plot(v)
    lines_plot.plot_stack(v)
