#!/usr/bin/env python

import requests
from datetime import datetime
import time
import sys

COLS = [
       'mc_1p0', 'mc_2p5', 'mc_4p0', 'mc_10p0', 'nc_0p5', 'nc_1p0', 'nc_2p5', 'nc_4p0', 'nc_10p0', 'size_um'
]

class AQIMonitor:
    def __init__(self, file):
        self.file = file
        self._fd = None

    def write(self, meas):
        if self._fd is None:
            self._fd = open(self.file, "a")
        vals = [str(meas.get(k)) for k in COLS]
        vals.insert(0, str(datetime.now()))
        self._fd.write(",".join(vals) + "\n")
        self._fd.flush()
    
    def read(self):
        try:
            result = requests.get("http://192.168.1.82:5555")
            result.raise_for_status()
            return result.json()
        except Exception as e:
            print(f"error reading data: {e}")
        return None
    
    def run(self):
        while True:
            data = self.read()
            if data:
                self.write(data)
            time.sleep(2)

    def __enter__(self):
        self._fd = open(self.file, "a")
        return self

    def __exit__(self, *a):
        if self._fd:
            self._fd.close()
            self._fd = None

if __name__ == "__main__":
    if len(sys.argv) == 2:
        filename = sys.argv[1]
    else:
        filename = "./aqi.csv"
    with AQIMonitor(filename) as am:
        am.run()

    

    


            

