import re
import os
import csv

class Parser:
  def __init__(self, location):
    self.location = location

  def parse_file(self, file): # must be implemented
    catch_param = {} # NOTE: the number must be the 2nd group
    
    # catch_param["naive-gen"] = re.compile(r"\[naive\] Generated (\d+) TXs in (\d+\.\d+)s \((\d+\.\d+)ns/TX\)")
    catch_param["naive-rep"] = re.compile(r"\[naive\] Replayed (\d+) TXs in (\d+\.\d+)s \((\d+\.\d+)ns/TX\)")
    # catch_param["link-gen"] = re.compile(r"\[forward_link\] Generated (\d+) TXs in (\d+\.\d+)s \((\d+\.\d+)ns/TX\)")
    catch_param["link-rep"] = re.compile(r"\[forward_link\] Replayed (\d+) TXs in (\d+\.\d+)s \((\d+\.\d+)ns/TX\)")
    # catch_param["seq-gen"] = re.compile(r"\[seq_log\] Generated (\d+) TXs in (\d+\.\d+)s \((\d+\.\d+)ns/TX\)")
    catch_param["seq-rep"] = re.compile(r"\[seq_log\] Replayed (\d+) TXs in (\d+\.\d+)s \((\d+\.\d+)ns/TX\)")
    o = {}
    # breakpoint()
    with open(file, "r") as f:
      for l in f.readlines():
        for k,v in catch_param.items():
          m = v.match(l)
          if m:
            # breakpoint()
            o["tx"] = float(m.group(1))
            o["time"] = float(m.group(2))
            o["latency"] = float(m.group(3))
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
        row = [l["params"][p] if p in l["params"] else l["eval"][p] for p in header]
        csvwriter.writerow(row)
