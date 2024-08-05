#!/bin/env python3

from common import BenchmarkParameters, CollectData
from parse_sol import Parser
from plot import LinesPlot, BackendDataset
# import numpy as np

# Besides the parameters below, the PM latency in:
# POWER8TM/backends/extra_MACROS.h (look up #define delay_for_pm)
# may be relevant

if __name__ == "__main__":
  
  # This class keeps the parameters for the benchmark.
  # Pass a list of arguments that need to be passed to the bechmark during the experiment
  params = BenchmarkParameters(["-u", "-d", "-i", "-r", "-n"])

  # Here set the possible values for each parameter (pass a list with valid values).
  # Note the experiment will run all possible combinations of arguments.
  # params.set_params("-u", [1,10,50])
  params.set_params("-u", [1, 10, 50])
  # params.set_params("-d", [2000])
  params.set_params("-d", [600000])
  params.set_params("-i", [50000, 200000, 800000])
  # params.set_params("-i", [1000])
  params.set_params("-r", [4000000])
  # params.set_params("-n", [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32])
  #params.set_params("-n", [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64])
  params.set_params("-n", [1, 4, 8, 16, 32, 64])

  # Set the number of times each run is repeated (for average/stardard deviation computation).
  nb_samples = 5

  # Set the location of the benchmark here. Each backend needs to be associated with
  # a benchmark (allows to compare with "exotic" implementations).
  locations = [
    "../POWER8TM/benchmarks/datastructures",
    "../POWER8TM/benchmarks/datastructures",
    "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    "../power8tm-pisces/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
  ]
  # The backend name goes here (don't forget to match the position in the
  # "backends" list with the position in the "locations" list)
  backends = [
    "psi",
    "psi-strong",
    "spht",
    "pisces",
    # "htm-sgl",
    # "htm-sgl-sr",
    # "si-htm",
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
    "spht-log-linking" : "SPHT-LL",
    "si-htm" : "SI-TM",
    "ureads-strong": "ureads-strong", 
    "ureads-p8tm": "ureads-p8tm"
  }

  # IMPORTANT: set the name of the dataset here, this folder needs to be
  # empty when taking new samples (else it can overwrite/append the stdout
  # of the new samples with the stdout of the old samples).
  data_folder = "data-rbtree-test"

  datasets_thr = {}
  datasets_aborts = {}

  # for-each pair <location,backend> collect data
  for loc,backend in zip(locations,backends):

    # repeat for-each sample
    for s in range(nb_samples):
      data = CollectData(
          loc,
          "redblacktree/redblacktree",
          "build-datastructures.sh",
          backend,
          f"{data_folder}/{backend}-s{s}"
        )
      
      # This line starts the benchmark and tests all combinations parameters.
      data.run_sample(params) # NOTE: comment if you already have the data and just want to refresh the plots.

      # Parses the stdout into a .csv that can be used for the plots.
      # Check the tests_py/parse_sol.py for the regular expressions that are catched in the stdout.
      parser = Parser(f"{data_folder}/{backend}-s{s}")
      parser.parse_all(f"{data_folder}/{backend}-s{s}.csv")

    # Creates the plots. In this case we want 1 plot for each combination <"-u","-i">.
    # for-each "-u" value
    for u in params.params["-u"]:
      # if backend == "htm-sgl" or backend == "p8tm-si-v2": # NOTE: this serves to ignore some lines in the plots
      #   continue
      if u not in datasets_thr:
        datasets_thr[u] = {}

      # for-each "-i" value
      for i in params.params["-i"]:
        if i not in datasets_thr[u]:
          datasets_thr[u][i] = []

        # Filters the data for the plot. In this case we are taking "-n" in the x-axis and
        # "-d"/"time" in the y-axis. Use a lambda function to take the required data into
        # each axis. The final argument is a dictionary with the filter of the dataset.
        # In this case we are looking for rows where "-u" == u AND "-i" == i.
        ds = BackendDataset(
        name_map[backend],
        [f"{data_folder}/{backend}-s{s}.csv" for s in range(nb_samples)],
        lambda e: e["-n"], "Nb. Threads",
        lambda e: e["-d"]/e["time"], "Throughput (T/s)",
        {"-u": u, "-i": i}
        )
        
        # Adds a bar plot for the abort type.          
        ds.add_stack("Abort types", "Percentage of txs", {
          "tx conflict": lambda e: (e["confl-trans"] + e["rot-trans-aborts"])/(e["total-commits"]+e["total-aborts"]),
          "non-tx conflict": lambda e: (e["confl-non-trans"] + e["rot-non-trans-aborts"])/(e["total-commits"]+e["total-aborts"]),
          "capacity": lambda e: (e["capac-aborts"] + e["rot-capac-aborts"])/(e["total-commits"]+e["total-aborts"]),
          "other": lambda e: (e["other-aborts"] + e["rot-other-aborts"] + e["confl-self"] + e["rot-self-aborts"] + e["user-aborts"] + e["rot-user-aborts"])/(e["total-commits"]+e["total-aborts"]),
        })


        ds.add_stack("Types of committed transactions", "Percentage of committed txs", {
            "non-tx commits": lambda e: (e["nontx-commits"])/(e["total-commits"]),
            "ROT commits": lambda e: (e["rot-commits"])/(e["total-commits"]),
            "HTM commits": lambda e: (e["htm-commits"])/(e["total-commits"]),
            "SGL commits": lambda e: (e["gl-commits"])/(e["total-commits"]),
            "STM commits": lambda e: (e["stm-commits"])/(e["total-commits"]),
          })
        
        # Adds a bar plot for the profile information.
        def divByUpdTxtime(e, attr):
          if (e["total-upd-tx-time"] == 0).any():
            return 0
          else:
            return (e[attr])
        ds.add_stack("Latency profile (update txs)", "Time (clock ticks)", {
          "processing committed txs.": lambda e: divByUpdTxtime(e, "total-upd-tx-time"),
          "isolation wait": lambda e: divByUpdTxtime(e, "total-sus-time"),
          "redo log flush": lambda e: divByUpdTxtime(e, "total-flush-time"),
          "durability wait": lambda e: divByUpdTxtime(e, "total-dur-commit-time"),
          # TODO
          # "proc. aborted txs": lambda e: divByUpdTxtime(e, "total-abort-upd-tx-time")
        })

        # Adds a bar plot for the profile information.
        def divByROTxtime(e, attr):
          if (e["total-ro-tx-time"] == 0).any():
            return 0
          else:
            return (e[attr])
            # return (e[attr] / (e["total-ro-tx-time"]))
        ds.add_stack("Latency profile (read-only txs)", "Time (clock ticks)", {
          "proc. committed txs": lambda e: divByROTxtime(e, "total-ro-tx-time"),
          "durability wait": lambda e: divByROTxtime(e, "total-ro-dur-wait-time"),
          # TODO
          # "proc. aborted txs": lambda e: divByROTxtime(e, "total-abort-ro-tx-time")
        })

        datasets_thr[u][i] += [ds]
    
  # this for-loop does the actual plotting (in the previous ones we are just
  # setting up the data that we want to plot).
  for u,v in datasets_thr.items():
    for i,w in v.items():
        lines_plot = LinesPlot(f"{u}% updates, {i/1000}k initial items", f"rbtree_thr_{u}upds_{i}items.pdf", figsize=(8, 4))
        
        # throughput plot
        lines_plot.plot(w)

        # abort+profiling plot
        lines_plot.plot_stack(w)
