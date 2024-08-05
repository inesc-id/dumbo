import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# Read data
NB_FILES = 10
files = [
    "replayer/naive-s{}.csv",
    "replayer/log_link-s{}.csv",
    "replayer/seq_log-s{}.csv"
]
solutions = [
    "SPHT",
    "SPHT-LL",
    "DUMBO"
]

X = []
Ys = []
err = []


for f in files:
    r = pd.DataFrame()
    for i in range(NB_FILES):
        aux = pd.read_csv(f.format(i), sep="\t").sort_values(by=["-n"])
        aux["throughput"] = 1e9 / aux["latency"]
        r = pd.concat((r, aux))
    if len(X) == 0:
        X = r["-n"].unique()
    Ys += [[r[r["-n"] == x].mean()["throughput"] for x in X]]
    err += [[r[r["-n"] == x].std()["throughput"] for x in X]]

# Create the first subplot
fig, axs = plt.subplots(figsize=(7,3), nrows=1, ncols=1)
axs.set_ylim([2e5,2.9e5])

for y,e,s in zip(Ys,err,solutions):
    axs.errorbar(X, y, yerr=e, label=s)

axs.set_ylabel('Throughput (TXs/s)')
axs.set_xlabel('Nb per-thread write logs')
#axs[1].set_xticklabels([])

# Adjust layout to prevent clipping of the second y-axis label
plt.subplots_adjust(left=0.12, right=0.985, top=0.925, bottom=0.145, hspace=0.05, wspace=0.05)
axs.margins(x=0.01)

# Show the plot
axs.legend(loc=(0.01, 0.05)) #bbox_to_anchor=(0.5, 0.83, 0.3, 0.8)
axs.set_title("Replay throughput")
plt.savefig(f"replayer_70_delay.pdf")
#plt.show()
