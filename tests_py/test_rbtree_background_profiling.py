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
  params.set_params("-u", [10])
  # params.set_params("-d", [2000])
  params.set_params("-d", [6000000])
  params.set_params("-i", [1000])
  # params.set_params("-i", [1000])
  params.set_params("-r", [10000000])
  # params.set_params("-n", [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32])
  #params.set_params("-n", [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64])
  params.set_params("-n", [1, 2, 4, 8, 16, 32])

  # Set the number of times each run is repeated (for average/stardard deviation computation).
  nb_samples = 5

  # Set the location of the benchmark here. Each backend needs to be associated with
  # a benchmark (allows to compare with "exotic" implementations).
  locations = [
    "../POWER8TM/benchmarks/datastructures",
    "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../power8tm-pisces/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
  ]
  # The backend name goes here (don't forget to match the position in the
  # "backends" list with the position in the "locations" list)
  backends = [
    # "psi",
    # "psi-strong",
    "spht",
    # "spht-log-linking",
    # "pisces",
    # "htm-sgl",
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
    "spht-log-linking" : "SPHT-LL",
    "si-htm" : "SI-TM",
    "ureads-strong": "ureads-strong", 
    "ureads-p8tm": "ureads-p8tm"
  }

  # IMPORTANT: set the name of the dataset here, this folder needs to be
  # empty when taking new samples (else it can overwrite/append the stdout
  # of the new samples with the stdout of the old samples).
  data_folder = "dataRBTreeProfiling"

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

        # TODO: fix pisces stdout
        if backend != "pisces":

          # Adds a bar plot for number of abort. The last argument is a dictionary with the label
          # and the the data from the dataset (use a lambda function to calculate).
          ds.add_stack("Commits vs Aborts", "Count", {
            "read-only commits": lambda e: e["read-commits"],
            "ROT commits": lambda e: e["rot-commits"],
            "HTM commits": lambda e: e["htm-commits"],
            "SGL commits": lambda e: e["gl-commits"],
            "aborts": lambda e: e["total-aborts"]
          })

          # Adds a bar plot for the abort type.
          ds.add_stack("Abort types", "Nb. aborts", {
            "tx conflict": lambda e: e["confl-trans"] + e["rot-trans-aborts"],
            "non-tx conflict": lambda e: e["confl-non-trans"] + e["rot-non-trans-aborts"],
            "capacity": lambda e: e["capac-aborts"] + e["rot-capac-aborts"],
            "other": lambda e: e["other-aborts"] + e["rot-other-aborts"] + e["confl-self"] + e["rot-self-aborts"] + e["user-aborts"] + e["rot-user-aborts"],
          })

          # Adds a bar plot for the profile information.
          def divByNumUpdTxs(e, attr):
            if (e["htm-commits"]+e["rot-commits"] == 0).any():
              return 0
            else:
              return (e[attr] / (e["htm-commits"]+e["rot-commits"]))
          ds.add_stack("Latency profile (update txs)", "Time (clock ticks)", {
            "processing committed txs.": lambda e: divByNumUpdTxs(e, "total-upd-tx-time"),
            "isolation wait": lambda e: divByNumUpdTxs(e, "total-sus-time"),
            "redo log flush": lambda e: divByNumUpdTxs(e, "total-flush-time"),
            "durability wait": lambda e: divByNumUpdTxs(e, "total-dur-commit-time"),
          })

          # Adds a bar plot for the profile information.
          def divByNumROTxs(e, attr):
            if (e["read-commits"] == 0).any():
              return 0
            else:
              return (e[attr] / (e["read-commits"]))
          ds.add_stack("Latency profile (read-only txs)", "Time (clock ticks)", {
            "tx proc.": lambda e: divByNumROTxs(e, "total-ro-tx-time"),
            "durability wait": lambda e: divByNumROTxs(e, "total-ro-dur-wait-time")
          })

          # Adds a bar plot for the profile information.
          # def normalize(e, attr):
          #   attrs = ["total-wait-time", "total-flush-time", "total-wait2-time", "total-commit-time", "total-abort-time"]
          #   do_sum = 0.00001
          #   for s in attrs:
          #     do_sum += (e[s]/e["-n"]) / (e["total-sum-time"])
          #   return (e[attr] / e["-n"]) / (e["total-sum-time"]) / do_sum
          # ds.add_stack("Profile information", "fraction of time", {
          #   "wait1": lambda e: normalize(e, "total-wait-time"),
          #   "sus-res": lambda e: normalize(e, "total-flush-time"),
          #   "wait2": lambda e: normalize(e, "total-wait2-time"),
          #   "commitTX": lambda e: normalize(e, "total-commit-time"),
          #   "abortedTX": lambda e: normalize(e, "total-abort-time")
          # })
        datasets_thr[u][i] += [ds]
    
  # this for-loop does the actual plotting (in the previous ones we are just
  # setting up the data that we want to plot).
  for u,v in datasets_thr.items():
    for i,w in v.items():
      lines_plot = LinesPlot(f"{u}% updates, {i/1000}k initial items", f"thr_{u}upds_{i}items.pdf", figsize=(8, 4))
      
      # throughput plot
      lines_plot.plot(w)

      # abort+profiling plot
      lines_plot.plot_stack(w)
