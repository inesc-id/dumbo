#!/bin/env python3

from common import BenchmarkParameters, CollectData
from parse_sol import Parser
from plot import LinesPlot, BackendDataset

if __name__ == "__main__":
  params = BenchmarkParameters(["-w", "-m", "-s", "-d", "-o", "-p", "-r", "-n", "-t"])
  params.set_params("-w", [32]) # nb warehouses
  params.set_params("-m", [32]) # max nb warehouses (put the same as -w)
  params.set_params("-t", [5])
  
  # From SI-HTM paper: mixed scenario
  params.set_params("-s", [4], True)
  params.set_params("-d", [4], True)
  params.set_params("-o", [4], True)
  params.set_params("-p", [43], True)
  params.set_params("-r", [45], True)

# #   From SI-HTM paper: read-dominated scenario
#   params.set_params("-s", [4], True)
#   params.set_params("-d", [4], True)
#   params.set_params("-o", [80], True)
#   params.set_params("-p", [4], True)
#   params.set_params("-r", [8], True)
  
  # # From SPHT paper
  # params.set_params("-s", [0], True)
  # params.set_params("-o", [0], True)
  # params.set_params("-p", [95], True)
  # params.set_params("-r", [2], True)
  # params.set_params("-d", [3], True)

  # params.set_params("-s", [4, 8], True) # to pass more than 1 combination of values
  # params.set_params("-d", [4, 4], True)
  # params.set_params("-o", [4, 4], True)
  # params.set_params("-p", [43, 39], True)
  # params.set_params("-r", [45, 45], True)
  params.set_params("-n", [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64])
  nb_samples = 5
  locations = [
    "../POWER8TM/benchmarks/tpcc",
    "../POWER8TM/benchmarks/tpcc",
    "../POWER8TM/benchmarks/tpcc",
    # # "../power8tm-pisces/benchmarks/tpcc",
    "../POWER8TM/benchmarks/tpcc",
    "../POWER8TM/benchmarks/tpcc",
    # "../POWER8TM/benchmarks/tpcc",
    # "../POWER8TM/benchmarks/tpcc",
  ]
  # The backend name goes here (don't forget to match the position in the
  # "backends" list with the position in the "locations" list)
  backends = [
    "psi",
    "psi-strong",
    "spht",
    # "pisces",
    "htm-sgl",
    # "htm-sgl-sr",
    "si-htm",
    # "ureads-strong",
    # "ureads-p8tm"
  ]

  # Label names in the plots
  name_map = {
    "psi" : "DUMBO-SI",
    "psi-strong" : "DUMBO-Opa",
    "pisces" : "Pisces",
    "htm-sgl" : "HTM",
    "htm-sgl-sr" : "HTM+sus",
    "spht" : "SPHT",
    "si-htm" : "SI-TM",
    "ureads-strong": "ureads-strong", 
    "ureads-p8tm": "ureads-p8tm"
  }
  
  data_folder = "dataTPCC_mixed"

  datasets_thr = {}
  datasets_aborts = {}
  for loc,backend in zip(locations,backends):
    for sample in range(nb_samples):
      data = CollectData(
          loc,
          "code/tpcc",
          "build-tpcc.sh",
          backend,
          f"{data_folder}/{backend}-s{sample}"
        )
      data.run_sample(params) # TODO: not running samples
      parser = Parser(f"{data_folder}/{backend}-s{sample}")
      parser.parse_all(f"{data_folder}/{backend}-s{sample}.csv")
    lst_each = params.list_for_each_param(["-s", "-d", "-o", "-p", "-r"])
    # print(lst_each)
    for s,d,o,p,r in lst_each:
      if (s,d,o,p,r) not in datasets_thr:
        datasets_thr[(s,d,o,p,r)] = []
      ds = BackendDataset(
        name_map[backend],
        [f"{data_folder}/{backend}-s{sample}.csv" for sample in range(nb_samples)],
        lambda e: e["-n"], "Nb. Threads",
        lambda e: e["txs-tpcc"]/e["time-tpcc"], "Throughput (T/s)",
        {"-s": s, "-d": d, "-o": o, "-p": p, "-r": r}
      )
      if backend != "pisces":
        ds.add_stack("Commits vs Aborts", "Count", {
          "ROT-commits": lambda e: e["rot-commits"] if "rot-commits" in e else 0,
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
      datasets_thr[(s,d,o,p,r)] += [ds]
    
  for u,v in datasets_thr.items():
    # print(u)
    # print(v)
    lines_plot = LinesPlot(f"[-s, -d, -o, -p, -r] = {u}", f"tpcc_{u}.pdf", figsize=(8, 4))
    lines_plot.plot(v)
    lines_plot.plot_stack(v)
