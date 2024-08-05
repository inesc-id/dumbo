import re
import os
import csv

class Parser:
  def __init__(self, location):
    self.location = location

  def parse_file(self, file): # must be implemented
    catch_param = {} # NOTE: the number must be the 2nd group
    catch_param["tpcc-StockLevel"] = re.compile(r"\s*StockLevel\s*:\s*(\d+\.\d+) (\d+)")
    catch_param["tpcc-Delivery"] = re.compile(r"\s*Delivery\s*:\s*(\d+\.\d+) (\d+)")
    catch_param["tpcc-OrderStatus"] = re.compile(r"\s*OrderStatus\s*:\s*(\d+\.\d+) (\d+)")
    catch_param["tpcc-Payment"] = re.compile(r"\s*Payment\s*:\s*(\d+\.\d+) (\d+)")
    catch_param["tpcc-NewOrder"] = re.compile(r"\s*NewOrder\s*:\s*(\d+\.\d+) (\d+)")
    catch_param["time"] = re.compile(r"\s*(Time) = (\d+\.\d+)")
    catch_param["txs-tpcc"] = re.compile(r"\s*(Txs): (\d+)")
    catch_param["time-tpcc"] = re.compile(r"\s*Total (time) \(secs\): (\d+\.\d+)")
    catch_param["pisces-aborts"] = re.compile(r"\s*Starts=(\d+) Aborts=(\d+)")
    catch_param["total-commits"] = re.compile(r"\s*Total (commits):\s+(\d+)")
    catch_param["htm-commits"] = re.compile(r"\s*HTM (commits):\s+(\d+)")
    catch_param["nontx-commits"] = re.compile(r"\s*Non-tx (commits):\s+(\d+)")
    catch_param["rot-commits"] = re.compile(r"\s*ROT (commits):\s+(\d+)")
    catch_param["stm-commits"] = re.compile(r"\s*STM (commits):\s+(\d+)")
    catch_param["gl-commits"] = re.compile(r"\s*GL (commits):\s+(\d+)")
    catch_param["total-aborts"] = re.compile(r"\s*Total (aborts):\s+(\d+)")
    catch_param["confl-aborts"] = re.compile(r"\s*HTM conflict (aborts):\s+(\d+)")
    catch_param["confl-trans"] = re.compile(r"\s*HTM trans (conflicts|aborts):\s+(\d+)")
    catch_param["confl-non-trans"] = re.compile(r"\s*HTM non-trans (conflicts|aborts):\s+(\d+)")
    catch_param["confl-self"] = re.compile(r"\s*HTM self (conflicts|aborts):\s+(\d+)")
    catch_param["capac-aborts"] = re.compile(r"\s*HTM capacity (aborts):\s+(\d+)")
    catch_param["persis-aborts"] = re.compile(r"\s*HTM persistent (aborts):\s+(\d+)")
    catch_param["user-aborts"] = re.compile(r"\s*HTM user (aborts)\s*:\s+(\d+)")
    catch_param["other-aborts"] = re.compile(r"\s*HTM other (aborts):\s+(\d+)")
    catch_param["rot-confl-aborts"] = re.compile(r"\s*ROT conflict (aborts):\s+(\d+)")
    catch_param["rot-self-aborts"] = re.compile(r"\s*ROT self (aborts):\s+(\d+)")
    catch_param["rot-trans-aborts"] = re.compile(r"\s*ROT trans (aborts):\s+(\d+)")
    catch_param["rot-non-trans-aborts"] = re.compile(r"\s*ROT non-trans (aborts):\s+(\d+)")
    catch_param["rot-other-confl-aborts"] = re.compile(r"\s*ROT other conflict (aborts):\s+(\d+)")
    catch_param["rot-user-aborts"] = re.compile(r"\s*ROT user (aborts):\s+(\d+)")
    catch_param["rot-capac-aborts"] = re.compile(r"\s*ROT capacity (aborts):\s+(\d+)")
    catch_param["rot-persis-aborts"] = re.compile(r"\s*ROT persistent (aborts):\s+(\d+)")
    catch_param["rot-other-aborts"] = re.compile(r"\s*ROT other (aborts):\s+(\d+)")
    catch_param["total-sum-time"] = re.compile(r"\s*Total sum (time):\s+(\d+)")
    catch_param["total-commit-time"] = re.compile(r"\s*Total commit (time):\s+(\d+)")
    catch_param["total-wait-time"] = re.compile(r"\s*Total wait (time):\s+(\d+)")
    catch_param["total-sus-time"] = re.compile(r"\s*Total sus (time):\s+(\d+)")
    catch_param["total-flush-time"] = re.compile(r"\s*Total flush (time):\s+(\d+)")
    catch_param["total-dur-commit-time"] = re.compile(r"\s*Total dur_commit (time):\s+(\d+)")
    catch_param["total-ro-dur-wait-time"] = re.compile(r"\s*Total RO_dur_wait (time):\s+(\d+)")
    catch_param["total-upd-tx-time"] = re.compile(r"\s*Total upd tx (time):\s+(\d+)")
    catch_param["total-ro-tx-time"] = re.compile(r"\s*Total RO tx (time):\s+(\d+)")
    catch_param["total-abort-upd-tx-time"] = re.compile(r"\s*Total abort-upd (time):\s+(\d+)")
    catch_param["total-abort-ro-tx-time"] = re.compile(r"\s*Total abort-ro (time):\s+(\d+)")

    o = {}
    with open(file, "r") as f:
      print(file)
      for l in f.readlines():
        for k,v in catch_param.items():
          m = v.match(l)
          if m:
            t = m.group(2)
            o[k] = float(t)
            break
    return o

  def parse_all(self, write_csv):
    r = re.compile(r"(\w)(.*)")
    lines = []
    for root, dirs, files in os.walk(self.location):
      for f in files:
        o = {}
        # assumes benchmark-p1234-k1234-...
        for s in f.split("-")[1:]:
          m = r.match(s)
          if m:
            o[f"-{m.group(1)}"] = m.group(2)
        lines += [{
          "params": o,
          "eval": self.parse_file(f"{root}/{f}")
        }]
    ### TODO: convert to CSV
    # breakpoint()
    # print(lines[0])
    header = list(lines[0]["params"].keys()) + list(lines[0]["eval"].keys())
    # print(header)
    # header = set(header_l[0])
    with open(write_csv, "w+", newline='') as csvfile:
      csvwriter = csv.writer(csvfile, delimiter='\t', quotechar='"', quoting=csv.QUOTE_MINIMAL)
      csvwriter.writerow(header)
      for l in lines:
        # print(l)
        row = [l["params"][p] if p in l["params"] else l["eval"][p] for p in header]
        csvwriter.writerow(row)
