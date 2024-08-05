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
fig, axs = plt.subplots(figsize=(7,3), nrows=3, ncols=1, sharex=True, gridspec_kw={'height_ratios': [1, 3, 1]})
# axs[0].spines.bottom.set_visible(False)
axs[0].spines.bottom.set_color("#AAAAAA")
axs[0].xaxis.tick_top()
axs[1].tick_params(bottom=False, top=False)
axs[1].spines.bottom.set_color("#AAAAAA")
axs[1].spines.top.set_color("#AAAAAA")
# axs[1].spines.top.set_visible(False)
# axs[1].spines.bottom.set_visible(False)
axs[2].spines.top.set_color("#AAAAAA")
# axs[2].spines.top.set_visible(False)
#axs[0].tick_params(labeltop=False)

kwargs = dict(marker=[(-1, -0.5), (1, 0.5)], markersize=12,
              linestyle="none", color='k', mec='k', mew=1, clip_on=False)

# 200000 items
max_y = 3e5
break2_y = 2.7e5
break1_y = 2.2e5
offset_y = 1

# 50000 items
#max_y = 2.0e7
#break_y = 4.5e6
#break_y_offset = 0.1e6

# 1M items
#max_y = 5.0e5
#break_y = 2.0e5
#break_y_offset = 0.1e5

axs[0].set_ylim([break2_y+offset_y,max_y]) # top
axs[1].set_ylim([break1_y,break2_y]) # middle
axs[2].set_ylim([0,break1_y-offset_y]) # below

axs[0].plot([0,65], [break2_y,break2_y], **kwargs)
axs[1].plot([0,65], [break2_y,break2_y], **kwargs)
axs[1].plot([0,65], [break1_y,break1_y], **kwargs)
axs[2].plot([0,65], [break1_y,break1_y], **kwargs)

for y,e,s in zip(Ys,err,solutions):
    axs[0].errorbar(X, y, yerr=e, label=s)
    axs[1].errorbar(X, y, yerr=e, label=s)
    axs[2].errorbar(X, y, yerr=e, label=s)

axs[1].set_ylabel('Throughput (TXs/s)')
axs[2].set_xlabel('Nb per-thread write logs')
#axs[1].set_xticklabels([])

# Adjust layout to prevent clipping of the second y-axis label
plt.subplots_adjust(left=0.095, right=0.985, top=0.925, bottom=0.145, hspace=0.05, wspace=0.05)
axs[0].margins(x=0)
axs[1].margins(x=0)
axs[2].margins(x=0)

# Show the plot
axs[2].legend(loc=(0.0, 0.05)) #bbox_to_anchor=(0.5, 0.83, 0.3, 0.8)
axs[0].set_title("Replay latency")
plt.savefig(f"replayer_70_delay.pdf")
#plt.show()
