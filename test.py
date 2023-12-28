import serial
import threading as th
import queue

class Comms:
    def __init__(self):
        self.serial = serial.Serial(
            port="/dev/ttyACM0", stopbits=serial.STOPBITS_ONE, parity=serial.PARITY_EVEN, baudrate=115200, timeout=1)
        self.serial.flush()
        self.ready = False
        self.cmd_queue = queue.Queue()
        self.rep_queue = queue.Queue()
        self.buf = ""

        th.Thread(target=self._background_task, daemon=True).start()

    def send_command(self, cmd: str):
        self.cmd_queue.put(cmd)

    def _background_task(self):
        while True:
            try:
                self._do_background_work()
            except KeyboardInterrupt:
                raise
            except Exception as e:
                print(f"error in worker: {e}")

    def _do_background_work(self):
        data = self.serial.read(10).decode("utf8")
        if not data:
            return
        buf = self.buf + data
        for line in buf.splitlines(keepends=True):
            if not line.endswith("\n"):
                self.buf = line
                return
            self._handle_line(line.strip())
        self.buf = ""
        if self.ready:
            while True:
                try:
                    cmd = self.cmd_queue.get_nowait()
                except queue.Empty:
                    break
                else:
                    self.serial.write(b">")
                    self.serial.flush()
                    rep = self.serial.read(1)
                    if rep.decode("utf8") == ">":
                        self.serial.write(cmd.encode("utf8") + b'\n')
                    else:
                        print(f"got garbage for command {rep}")

    def _handle_line(self, line: str):
        if line.startswith("L"):
            print(line[2:])
        elif line.startswith("R"):
            self.ready = True
        elif line.startswith("C"):
            self.rep_queue.put_nowait(line[2:])
        else:
            print(f"got garbage: {line}")


if __name__ == "__main__":
    comms = Comms()
    comms.send_command("hello")
    print(comms.rep_queue.get())