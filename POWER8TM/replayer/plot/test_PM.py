#!/bin/env python3

from common import BenchmarkParameters, CollectData
from parse_sol import Parser
from plot import LinesPlot, BackendDataset

if __name__ == "__main__":
  
  # This class keeps the parameters for the benchmark.
  # Pass a list of arguments that need to be passed to the bechmark during the experiment
  params = BenchmarkParameters(["-n", "-d"])

  # Here set the possible values for each parameter (pass a list with valid values).
  # Note the experiment will run all possible combinations of arguments.
  params.set_params("-d", [70])
  params.set_params("-n", [1, 2, 8, 16, 24, 32, 48, 64])

  # Set the number of times each run is repeated (for average/stardard deviation computation).
  nb_samples = 10

  # Set the location of the benchmark here. Each backend needs to be associated with
  # a benchmark (allows to compare with "exotic" implementations).
  locations = [
    "..",
    "..",
    ".."
  ]
  # The backend name goes here (don't forget to match the position in the
  # "backends" list with the position in the "locations" list)
  backends = [
    "naive",
    "log_link",
    "seq_log"
  ]

  # Label names in the plots
  name_map = {
    "naive" : "Naive",
    "log_link" : "Linking",
    "seq_log" : "Sequential"
  }

  map_nops_to_ns = {
    0 : 0,
    10 : 45, # assumes NB_NOPS * 4.5
    46 : 200,
    66 : 295,
    100 : 435
  }

  # IMPORTANT: set the name of the dataset here, this folder needs to be
  # empty when taking new samples (else it can overwrite/append the stdout
  # of the new samples with the stdout of the old samples).
  data_folder = "replayer"

  datasets_thr = {}
  datasets_aborts = {}

  # for-each pair <location,backend> collect data
  for loc,backend in zip(locations,backends):

    # repeat for-each sample
    for s in range(nb_samples):
      data = CollectData(
          loc,
          "test",
          "build.sh",
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
    for d in params.params["-d"]:
      # if backend == "htm-sgl" or backend == "p8tm-si-v2": # NOTE: this serves to ignore some lines in the plots
      #   continue
      if d not in datasets_thr:
        datasets_thr[d] = []

      # Filters the data for the plot. In this case we are taking "-n" in the x-axis and
      # "-d"/"time" in the y-axis. Use a lambda function to take the required data into
      # each axis. The final argument is a dictionary with the filter of the dataset.
      # In this case we are looking for rows where "-u" == u AND "-i" == i.
      ds = BackendDataset(
        name_map[backend],
        [f"{data_folder}/{backend}-s{s}.csv" for s in range(nb_samples)],
        lambda e: e["-n"], "Number of per-thread write logs",
        lambda e: e["latency"], "Replay duration (ns/TX)",
        {"-d": d}
      )
      datasets_thr[d] += [ds]
    
  # this for-loop does the actual plotting (in the previous ones we are just
  # setting up the data that we want to plot).
  for n,w in datasets_thr.items():
    lines_plot = LinesPlot(f"{n} worker threads (write logs)", f"repl_{n}_worker_thrs.pdf", figsize=(8, 4))
    
    # throughput plot
    lines_plot.plot(w)

    # abort+profiling plot
    # lines_plot.plot_stack(w)
