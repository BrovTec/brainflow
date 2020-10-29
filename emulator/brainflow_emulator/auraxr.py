import subprocess
import threading
import socket
import enum
import logging
import sys
import random
import time
import struct

from brainflow_emulator.emulate_common import TestFailureError, log_multilines


class State (enum.Enum):
    wait = 'wait'
    stream = 'stream'


class Message (enum.Enum):
    start_stream = b'b'
    stop_stream = b's'


def test_socket (cmd_list):
    logging.info ('Running %s' % ' '.join ([str (x) for x in cmd_list]))
    process = subprocess.Popen (cmd_list, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    stdout, stderr = process.communicate ()

    log_multilines (logging.info, stdout)
    log_multilines (logging.info, stderr)

    if process.returncode != 0:
        raise TestFailureError ('Test failed with exit code %s' % str (process.returncode), process.returncode)

    return stdout, stderr


def run_socket_server ():
    novaxr_thread = AuraXREmulator ()
    novaxr_thread.start ()
    return novaxr_thread


class AuraXREmulator (threading.Thread):

    def __init__ (self):
        threading.Thread.__init__ (self)

    def run (self):
        self.local_ip = '127.0.0.1'
        self.local_port = 2390
        self.server_socket = socket.socket (family = socket.AF_INET, type = socket.SOCK_STREAM)
        self.server_socket.bind ((self.local_ip, self.local_port))
        self.server_socket.listen (1)
        self.conn, self.client = self.server_socket.accept ()
        self.conn.settimeout (0.1)
        self.state = State.wait.value
        self.addr = None
        self.package_num = 0
        self.transaction_size = 19
        self.package_size = 72
        self.keep_alive = True

        while self.keep_alive:
            try:
                msg, self.addr = self.conn.recvfrom (128)
                if msg == Message.start_stream.value:
                    self.state = State.stream.value
                elif msg == Message.stop_stream.value:
                    self.state = State.wait.value
                if msg:
                    logging.warn ('received string %s', str (msg))
            except socket.timeout:
                logging.debug ('timeout for recv')
            except Exception:
                break

            if self.state == State.stream.value:
                package = list ()
                for _ in range (19):
                    package.append (self.package_num)
                    self.package_num = self.package_num + 1
                    if self.package_num % 256 == 0:
                        self.package_num = 0
                    for i in range (1, self.package_size - 8):
                        package.append (random.randint (0, 255))
                    timestamp = bytearray (struct.pack ("d", time.time ()))
                    package.extend (timestamp)
                try:
                    self.conn.send (bytes (package))
                except socket.timeout:
                    logging.info ('timeout for send')


def main (cmd_list):
    if not cmd_list:
        raise Exception ('No command to execute')
    server_thread = run_socket_server ()
    time.sleep (3)
    test_socket (cmd_list)
    server_thread.keep_alive = False
    server_thread.join ()


if __name__=='__main__':
    logging.basicConfig (format = '%(asctime)s %(levelname)-8s %(message)s', level = logging.INFO)
    main (sys.argv[1:])
