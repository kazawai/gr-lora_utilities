#!/usr/bin/env python

#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2024 KazaWai.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#


import datetime as dt
import time
from sigmf import SigMFFile
from gnuradio import gr
import socket
import os
import pmt
import threading
import numpy as np


class file_writer(gr.basic_block):
    """
    docstring for block file_writer
    """

    def __init__(
        self,
        filename,
        author,
        description,
        item_size,
        item_type,
        sample_rate,
        frequency,
        hw,
        version,
        is_loopback=False,
        ip_address="localhost",
        port=12345,
        num_inputs=1,
        **kwargs,
    ):

        # Init sync block with variale number of inputs
        gr.basic_block.__init__(
            self,
            name="file_writer",
            in_sig=[(np.complex64, 1) for _ in range(num_inputs)],
            out_sig=None,
        )

        # Register message input
        if num_inputs == 1:
            self.message_port_register_in(pmt.intern("detected"))
            self.set_msg_handler(pmt.intern("detected"), self.handle_msg_0)
        else:
            for i in range(num_inputs):
                self.message_port_register_in(pmt.intern(f"detected{i}"))

                # Passing the port id to the handler
                func = getattr(self, f"handle_msg_{i}")
                self.set_msg_handler(pmt.intern(f"detected{i}"), func)

        print("Message port(s) registered")

        self.filename = filename
        self.cur_filename = filename
        self.author = author
        self.description = description
        self.item_size = item_size
        self.item_type = item_type
        self.sample_rate = sample_rate
        self.frequency = frequency
        self.hw = hw
        self.version = version
        self.is_loopback = is_loopback
        self.new_symol = [True for _ in range(num_inputs)]
        self.total_symbols = [0 for _ in range(num_inputs)]
        self.n_inputs = num_inputs

        self.ip_address = ip_address
        self.port = port

        print(self.port)

        self.meta: list[SigMFFile] = [SigMFFile() for _ in range(self.n_inputs)]
        self.nitems_written = [0 for _ in range(self.n_inputs)]

        if item_size == 8:
            self.datatype = "cf32_le"
        elif item_size == 4:
            self.datatype_str = "rf32_le"
        elif item_size == 2 and item_type:
            self.datatype_str = "ci16_le"
        elif item_size == 2 and not item_type:
            self.datatype_str = "ri16_le"
        else:
            raise ValueError("Unsupported item type")

        self.device_id = 0

        if self.is_loopback:
            print("Connecting to socket")
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.host = socket.gethostname()
            self.sock.bind((self.ip_address, self.port))
            self.sock.listen(5)
            self.conn, self.addr = self.sock.accept()
            print("Connected to socket")

            # Start the thread to handle the device id
            self.thread = threading.Thread(target=self._thread_handle_device_id)
            self.thread.start()
        else:
            # If any input file already exists, append a timestamp to the filename
            if any(
                [
                    os.path.exists(self.cur_filename + f"_input{i}.sigmf-data")
                    for i in range(self.n_inputs)
                ]
            ):
                self.cur_filename = f"{self.cur_filename}_{int(time.time())}"
            if self.start():
                print(f"Started writing to {self.cur_filename}")

    def update_device_and_create_files(self, device_id=0):
        if self.device_id != device_id:
            print(f"Device id changed from {self.device_id} to {device_id}")
            if self.device_id != 0:
                # Add the total number of symbols to the metadata
                for i in range(self.n_inputs):
                    self.meta[i].set_global_info(
                        {
                            SigMFFile.DATATYPE_KEY: self.datatype,
                            SigMFFile.SAMPLE_RATE_KEY: self.sample_rate,
                            SigMFFile.DESCRIPTION_KEY: self.description,
                            SigMFFile.AUTHOR_KEY: self.author,
                            SigMFFile.DATASET_KEY: f"{self.cur_filename}_input{i}.sigmf-data",
                            SigMFFile.HW_KEY: self.hw,
                            SigMFFile.VERSION_KEY: self.version,
                            SigMFFile.COMMENT_KEY: f"Total number of symbols: {self.total_symbols[i]}",
                        }
                    )
                    self.meta[i].tofile(
                        self.cur_filename + "_input" + str(i) + ".sigmf-meta"
                    )
                    self.meta[i] = SigMFFile()
            self.device_id = device_id
            self.total_symbols = [0 for _ in range(self.n_inputs)]
            self.nitems_written = [0 for _ in range(self.n_inputs)]
            self.new_symol: list[bool] = [True for _ in range(self.n_inputs)]

        print(f"Received device id: {self.device_id}")
        self.conn.sendall(b"ACK")

        self.cur_filename = f"{self.filename}_device_{self.device_id}"
        # If the file exists, append a timestamp to the filename
        if os.path.exists(self.cur_filename + ".sigmf-data"):
            self.cur_filename = f"{self.cur_filename}_{int(time.time())}"

    def add_annotation(self, index, metadata, port_id, count=(256 * 13 * 4)):
        print(f"Adding annotation at index {index}")
        self.meta[port_id].add_annotation(index, count, metadata)

    def handle_msg_0(self, msg):
        self.handle_msg(msg, 0)

    def handle_msg_1(self, msg):
        self.handle_msg(msg, 1)

    def handle_msg_2(self, msg):
        self.handle_msg(msg, 2)

    def handle_msg(self, msg, port_id):
        print(f"Received message: {pmt.to_python(msg)} from port {port_id}")
        if pmt.to_python(msg) == True:
            self.new_symol[port_id] = True

    def _thread_handle_device_id(self):
        while True:
            try:
                device_id = self.conn.recv(1024).decode("utf-8")
                if device_id == "close":
                    print("Closing connection")
                    for i in range(self.n_inputs):
                        self.meta[i].set_global_info(
                            {
                                SigMFFile.DATATYPE_KEY: self.datatype,
                                SigMFFile.SAMPLE_RATE_KEY: self.sample_rate,
                                SigMFFile.DESCRIPTION_KEY: self.description,
                                SigMFFile.AUTHOR_KEY: self.author,
                                SigMFFile.DATASET_KEY: f"{self.cur_filename}_input{i}.sigmf-data",
                                SigMFFile.HW_KEY: self.hw,
                                SigMFFile.VERSION_KEY: self.version,
                                SigMFFile.COMMENT_KEY: f"Total number of symbols: {self.total_symbols[i]}",
                            }
                        )

                        if self.nitems_written[i] == 0:
                            print(
                                f"No data written to file: {self.cur_filename}_input{i}.sigmf-data"
                            )
                            continue

                        self.meta[i].tofile(
                            self.cur_filename + "_input" + str(i) + ".sigmf-meta"
                        )
                    self.conn.close()
                    os._exit(0)
                else:
                    self.conn.sendall(b"ACK")
                    self.update_device_and_create_files(int(device_id))
            except BlockingIOError:
                pass

    def general_work(self, input_items, output_items):
        n_inputs = len(input_items)
        assert (
            n_inputs == self.n_inputs
        ), f"Number of inputs: {n_inputs}. Excpected: {self.n_inputs}"
        print(f"Number of inputs: {n_inputs}")
        if len(self.meta) < n_inputs:
            self.meta = self.meta + [
                SigMFFile() for _ in range(n_inputs - len(self.meta))
            ]

        # Check if the input is empty
        if all([len(input_items[i]) == 0 for i in range(n_inputs)]):
            return 0
        else:  # Write to file
            for i in range(n_inputs):
                if len(input_items[i]) == 0:
                    print(f"Input {i} is empty")
                    continue

                if self.new_symol[i]:
                    self.total_symbols[i] += 1
                    self.add_annotation(
                        self.nitems_written[i],
                        {
                            SigMFFile.ANNOTATION_KEY: {
                                SigMFFile.FREQUENCY_KEY: self.frequency,
                                SigMFFile.DATETIME_KEY: dt.datetime.now().isoformat()
                                + "Z",
                                SigMFFile.COMMENT_KEY: f"LoRa Symbol {self.total_symbols[i]}",
                            }
                        },
                        i,
                    )
                # Write to file
                with open(self.cur_filename + f"_input{i}.sigmf-data", "ab") as f:
                    np.array(input_items[i]).tofile(f)

                self.new_symol[i] = False
                self.nitems_written[i] += len(input_items[i])
                self.consume(i, len(input_items[i]))

        return 0

    def stop(self):
        if all(self.nitems_written[i] == 0 for i in range(self.n_inputs)):
            print("No data written")
            return True

        for i in range(self.n_inputs):
            self.meta[i].set_global_info(
                {
                    SigMFFile.DATATYPE_KEY: self.datatype,
                    SigMFFile.SAMPLE_RATE_KEY: self.sample_rate,
                    SigMFFile.DESCRIPTION_KEY: self.description,
                    SigMFFile.AUTHOR_KEY: self.author,
                    SigMFFile.DATASET_KEY: f"{self.cur_filename}_input{i}.sigmf-data",
                    SigMFFile.HW_KEY: self.hw,
                    SigMFFile.VERSION_KEY: self.version,
                    SigMFFile.COMMENT_KEY: f"Total number of symbols: {self.total_symbols[i]}",
                }
            )

        for i in range(self.n_inputs):
            if self.nitems_written[i] == 0:
                print(
                    f"No data written to file: {self.cur_filename}_input{i}.sigmf-data"
                )
                continue
            self.meta[i].tofile(self.cur_filename + f"_input{i}.sigmf-meta")
            print(f"Metadata written to file: {self.cur_filename}_input{i}.sigmf-meta")

        return True
