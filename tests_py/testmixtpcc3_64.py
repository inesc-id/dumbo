#!/bin/env python3

from common import BenchmarkParameters, CollectData
from parse_sol import Parser
from plot import LinesPlot, BackendDataset

if __name__ == "__main__":
  params = BenchmarkParameters(["-w", "-m", "-s", "-d", "-o", "-p", "-r", "-n", "-t"])
  
  params.set_params("-s", [5], True)   
  params.set_params("-d", [5], True)
  params.set_params("-o", [5], True)
  params.set_params("-p", [43], True)
  params.set_params("-r", [42], True)
  data_folder = "datamixtpcc3_64"


  params.set_params("-w", [64]) # nb warehouses
  params.set_params("-m", [64]) # max nb warehouses (put the same as -w)
  params.set_params("-t", [5])

  params.set_params("-n", [1, 2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64])
  nb_samples = 3

  # params.set_params("-n", [1, 2, 4, 8, 16, 32, 64])
  # nb_samples = 1
  locations = [
   "../POWER8TM/benchmarks/tpcc",
   "../POWER8TM/benchmarks/tpcc",
  #  "../POWER8TM/benchmarks/tpcc",
  #  "../POWER8TM/benchmarks/tpcc",
  #  "../POWER8TM/benchmarks/tpcc",
  #  "../POWER8TM/benchmarks/tpcc",
  # #  "../POWER8TM/benchmarks/tpcc",
  #   "../power8tm-pisces/benchmarks/tpcc",
    # "../POWER8TM/benchmarks/tpcc",
#     "../POWER8TM/benchmarks/tpcc",
  ]
  # The backend name goes here (don't forget to match the position in the
  # "backends" list with the position in the "locations" list)
  backends = [
   "psi",
   "psi-strong",
  #  "htm-sgl",
  #  "si-htm",
  #  "spht",
  #  "spht-log-linking", 
  # #  "spht-quiescence-naive",
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
    "spht-quiescence-naive": "DUMBO-naive",
  }
  
 
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

      # if ((backend != "htm-sgl") and  (backend != "si-htm") ):

      def filter_threads(t) -> bool:
        x, y, sd = t
        # return True on the threads to keep
        return True if x in [2, 4, 8, 16] else False

          
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
      
      datasets_thr[(s,d,o,p,r)] += [ds]

  colors = {
    "SGL commit" : "#a83232",
    "Abort": "#404040", 
    # "DUMBO-opaq" : "#AF2126",
    # "DUMBO-SI" : "#F99F1E",
    # # "DUMBO-read" : "#9999ff",
    # "Pisces" : "#D3BEDA", 

  }
    
  for u,v in datasets_thr.items():
    # print(u)
    # print(v)
    lines_plot = LinesPlot(f"[-s, -d, -o, -p, -r] = {u}", f"tpcc_{u}.pdf", figsize=(8, 4), colors=colors)
    lines_plot.plot(v)
    lines_plot.plot_stack(v, filter_out_backends=["HTM", "SI-HTM", "Pisces", "SPHT-LL"])
