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
  params = BenchmarkParameters(["-u", "-d", "-i", "-r", "-n", "-b"])

  # Here set the possible values for each parameter (pass a list with valid values).
  # Note the experiment will run all possible combinations of arguments.
  params.set_params("-u", [10])
  params.set_params("-b", [512])
  # params.set_params("-d", [2000])
  params.set_params("-d", [6000000])
  params.set_params("-i", [50000])
  # params.set_params("-i", [1000])
  params.set_params("-r", [2000000])
  

  # params.set_params("-n", [1, 2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64])
  # nb_samples = 3

  params.set_params("-n", [1, 2, 4, 8, 16, 32, 64])
  nb_samples = 1

  # IMPORTANT: set the name of the dataset here, this folder needs to be
  # empty when taking new samples (else it can overwrite/append the stdout
  # of the new samples with the stdout of the old samples).
  data_folder = "data-hashmap"

  locations = [
   "../POWER8TM/benchmarks/datastructures",
   "../POWER8TM/benchmarks/datastructures",
  #  "../POWER8TM/benchmarks/datastructures",
  #  "../POWER8TM/benchmarks/datastructures",
  #  "../POWER8TM/benchmarks/datastructures",
  #  "../POWER8TM/benchmarks/datastructures",
  # #  "../POWER8TM/benchmarks/datastructures",
  #   "../power8tm-pisces/benchmarks/datastructures",
    # "../POWER8TM/benchmarks/datastructures",
#     "../POWER8TM/benchmarks/datastructures",
  ]
  # The backend name goes here (don't forget to match the position in the
  # "backends" list with the position in the "locations" list)
  backends = [
  #  "psi",
  #  "psi-strong",
  #  "htm-sgl",
  #  "si-htm",
   "spht",
  #  "spht-log-linking", 
  "spht-quiescence-naive2",
  # "spht-quiescence-naive2-strong",
  #  "pisces",
  #  "psi-bug",
  #  "psi-strong-bug",
  #  "spht-dumbo-readers",
  #  "pstm",
  #  "psi",
    # "htm-sgl",
    # "htm-sgl-sr",
    # "si-htm",
    # "ureads-strong",
    # "ureads-p8tm"
  ]
# Label names in the plots
  name_map = {
    "psi" : "DUMBO-SI",
    "psi-strong" : "DUMBO-opaq",
    "psi-bug" : "DUMBO-SI-bug",
    "psi-strong-bug" : "DUMBO--opaq-bug",
    "spht-dumbo-readers" : "DUMBO-read",
    "spht" : "SPHT",
    "pstm" : "PSTM", 
    "spht-log-linking" : "SPHT-LL",
    "pisces" : "Pisces",
    "htm-sgl" : "HTM",
    "htm-sgl-sr" : "HTM+sus",
    "si-htm" : "SI-HTM",
    "ureads-strong": "ureads-strong", 
    "ureads-p8tm": "ureads-p8tm",
    "spht-quiescence-naive2": "SPHT+SIHTM",
    "spht-quiescence-naive2-strong": "DUMBO-naive-opaq",
  }
  
 
  datasets_thr = {}
  datasets_aborts = {}

  # for-each pair <location,backend> collect data
  for loc,backend in zip(locations,backends):

    # repeat for-each sample
    for s in range(nb_samples):
      data = CollectData(
          loc,
          "hashmap/hashmap",
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
          datasets_thr[u][i] = {}

        # for-each "-b" value
        for b in params.params["-b"]:
          if b not in datasets_thr[u][i]:
            datasets_thr[u][i][b] = []
          # Filters the data for the plot. In this case we are taking "-n" in the x-axis and
          # "-d"/"time" in the y-axis. Use a lambda function to take the required data into
          # each axis. The final argument is a dictionary with the filter of the dataset.
          # In this case we are looking for rows where "-u" == u AND "-i" == i.
          ds = BackendDataset(
            name_map[backend],
            [f"{data_folder}/{backend}-s{s}.csv" for s in range(nb_samples)],
            lambda e: e["-n"], "Nb. Threads",
            lambda e: e["-d"]/e["time"], "Throughput (T/s)",
            {"-u": u, "-i": i, "-b": b}
          )

      def filter_threads(t) -> bool:
        x, y, sd = t
        # return True on the threads to keep
        return True if x in [2, 4, 8, 16, 32, 64] else False

          
      ds.add_stack("Prob. of different outcomes for a transaction", "Percentage of started transactions", {
          "non-tx commit": lambda e: (e["nontx-commits"])/(e["total-commits"]+e["total-aborts"]),
          "ROT commit": lambda e: (e["rot-commits"])/(e["total-commits"]+e["total-aborts"]),
          "HTM commit": lambda e: (e["htm-commits"])/(e["total-commits"]+e["total-aborts"]),
          "SGL commit": lambda e: (e["gl-commits"])/(e["total-commits"]+e["total-aborts"]),
          "STM commit": lambda e: (e["stm-commits"])/(e["total-commits"]+e["total-aborts"]),
          "Abort": lambda e: (e["total-aborts"])/(e["total-commits"]+e["total-aborts"]),
        }, is_percent=True, filter_x_fn=filter_threads)
    
      def divByAborts(e, attr):
        # if (e["total-aborts"] == 0).any():
        #   return 0
        # else:
        return (e[attr]/(e["total-aborts"]+e["total-commits"]-e["gl-commits"]))

                
      # Adds a bar plot for the abort type.          
      ds.add_stack("Abort types", "Percentage of aborts", {
        "tx conflict": lambda e: (divByAborts(e, "confl-trans") + divByAborts(e, "rot-trans-aborts")),
        "non-tx conflict": lambda e: (divByAborts(e, "confl-non-trans") + divByAborts(e, "rot-non-trans-aborts")),
        "capacity": lambda e: (divByAborts(e, "capac-aborts") + divByAborts(e, "rot-capac-aborts")), 
        "other": lambda e: (divByAborts(e, "other-aborts") + divByAborts(e, "confl-self") + divByAborts(e, "rot-self-aborts") + divByAborts(e, "user-aborts") + divByAborts(e, "rot-user-aborts")), 
      }, is_percent=True, fix_100=True, filter_x_fn=filter_threads)

      
      # Adds a bar plot for the profile information.
      def checkIfUpdCommitStats(e, attr):
        if (e["total-upd-tx-time"] == 0).any():
          return [0 for i in range(len(e))]
        else:
          return (e[attr]/e["total-upd-tx-time"])
      def checkIfAbortStats(e, attr):
        if (e["total-abort-upd-tx-time"] == 0).any():
          return [0 for i in range(len(e))]
        if (e["total-upd-tx-time"] == 0).any():
          return [0 for i in range(len(e))]
        else:
          return (e[attr]/e["total-upd-tx-time"])
      ds.add_stack("Latency profile (update txs)", "Overhead over time processing txs.", {
        #"processing committed txs.": lambda e: checkIfUpdCommitStats(e, "total-upd-tx-time"),
        "isolation wait": lambda e: (checkIfUpdCommitStats(e, "total-sus-time")),
        "suspend/resume": lambda e: (checkIfUpdCommitStats(e, "total-sus-time")),
        "redo log flush": lambda e: (checkIfUpdCommitStats(e, "total-flush-time")),
        "durability wait": lambda e: (checkIfUpdCommitStats(e, "total-dur-commit-time")),
        # "proc. aborted txs": lambda e: (checkIfAbortStats(e, "total-abort-upd-tx-time")),
      }, is_percent=True, filter_x_fn=filter_threads)

      # Adds a bar plot for the profile information.
      def checkIfROCommitStats(e, attr):
        if (e["total-ro-tx-time"] == 0).any():
          return [0 for i in range(len(e))]
        else:
          return (e[attr]/e["total-ro-tx-time"])
      def checkIfROAbortStats(e, attr):
        if (e["total-abort-ro-tx-time"] == 0).any():
          return [0 for i in range(len(e))]
        if (e["total-ro-tx-time"] == 0).any():
          return [0 for i in range(len(e))]
        else:
          return (e[attr]/e["total-ro-tx-time"])
      ds.add_stack("Latency profile (read-only txs)", "Overhead over time processing txs", {
        # "proc. committed txs": lambda e: checkIfROCommitStats(e, "total-ro-tx-time")),
        "durability wait": lambda e: (checkIfROCommitStats(e, "total-ro-dur-wait-time")),
        # "proc. aborted txs": lambda e: (checkIfROAbortStats(e, "total-abort-ro-tx-time"))
      }, is_percent=True, filter_x_fn=filter_threads)


      datasets_thr[u][i][b] += [ds]
    
  colors = {
    "SGL commit" : "#a83232",
    "Abort": "#404040", 
    # "DUMBO-opaq" : "#AF2126",
    # "DUMBO-SI" : "#F99F1E",
    # # "DUMBO-read" : "#9999ff",
    # "Pisces" : "#D3BEDA", 

  }
  # this for-loop does the actual plotting (in the previous ones we are just
  # setting up the data that we want to plot).
  for u,v in datasets_thr.items():
    for i,z in v.items():
      for b,w in z.items():
        lines_plot = LinesPlot(f"{u}% updates, {i/1000}k initial items", f"hashmap_thr_{u}upds_{i}items_{b}buckets.pdf", figsize=(8, 4), colors=colors)
        
        # throughput plot
        lines_plot.plot(w)

        # abort+profiling plot
        lines_plot.plot_stack(w, filter_out_backends=["HTM", "SI-HTM", "Pisces", "SPHT-LL"])
