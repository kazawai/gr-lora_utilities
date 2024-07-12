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
from gnuradio import blocks
import socket
import os
import pmt
import numpy as np


class file_writer(gr.sync_block):
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
        **kwargs,
    ):

        gr.sync_block.__init__(
            self, name="file_writer", in_sig=[np.complex64], out_sig=None
        )

        # Register message input
        self.message_port_register_in(pmt.intern("detected"))
        self.set_msg_handler(pmt.intern("detected"), self.handle_msg)
        print("Message port registered")

        self.filename = filename
        self.author = author
        self.description = description
        self.item_size = item_size
        self.item_type = item_type
        self.sample_rate = sample_rate
        self.frequency = frequency
        self.hw = hw
        self.version = version
        self.is_loopback = is_loopback
        self.new_symol = True
        self.total_symbols = 0

        self.ip_address = ip_address
        self.port = port

        self.meta = SigMFFile()
        self.nitems_written = 0

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
            self.host = socket.gethostname()
            self.sock.bind((self.host, self.port))
            self.sock.listen(5)
            self.conn, self.addr = self.sock.accept()
            self.conn.setblocking(False)
            print("Connected to socket")

            try:
                device_id = self.conn.recv(1024).decode("utf-8")
                if device_id == "close":
                    print("Closing connection")
                    self.meta.tofile(self.filename + ".sigmf-meta")
                    self.conn.close()
                    os._exit(0)
                else:
                    self.update_device_and_create_files(int(device_id))
            except BlockingIOError:
                pass

        else:
            self.meta = SigMFFile(
                global_info={
                    SigMFFile.DATATYPE_KEY: self.datatype,
                    SigMFFile.SAMPLE_RATE_KEY: self.sample_rate,
                    SigMFFile.DESCRIPTION_KEY: self.description,
                    SigMFFile.AUTHOR_KEY: self.author,
                    SigMFFile.DATASET_KEY: f"{filename}.sigmf-data",
                    SigMFFile.HW_KEY: self.hw,
                    SigMFFile.VERSION_KEY: self.version,
                },
            )
            self.start()

    def update_device_and_create_files(self, device_id=0):
        if self.device_id != device_id:
            print(f"Device id changed from {self.device_id} to {device_id}")
            if self.device_id != 0:
                # Add the total number of symbols to the metadata
                self.meta.set_global_info(
                    {
                        SigMFFile.DATATYPE_KEY: self.datatype,
                        SigMFFile.SAMPLE_RATE_KEY: self.sample_rate,
                        SigMFFile.DESCRIPTION_KEY: self.description,
                        SigMFFile.AUTHOR_KEY: self.author,
                        SigMFFile.DATASET_KEY: f"{self.filename}.sigmf-data",
                        SigMFFile.HW_KEY: self.hw,
                        SigMFFile.VERSION_KEY: self.version,
                        SigMFFile.COMMENT_KEY: f"Total number of symbols: {self.total_symbols}",
                    }
                )
                self.meta.tofile(self.filename + ".sigmf-meta")
                self.meta = SigMFFile()
            self.device_id = device_id

        print(f"Received device id: {self.device_id}")

        self.filename = f"{self.filename}_device_{self.device_id}"
        # If the file exists, append a timestamp to the filename
        if os.path.exists(self.filename + ".sigmf-data"):
            self.filename = f"{self.filename}_{int(time.time())}"

    def add_annotation(self, index, metadata, count=(256 * 13 * 4)):
        print(f"Adding annotation at index {index}")
        self.meta.add_annotation(index, count, metadata)

    def handle_msg(self, msg):
        print(f"Received message: {pmt.to_python(msg)}")
        if pmt.to_python(msg) == True:
            self.new_symol = True

    def work(self, input_items, output_items):
        if self.is_loopback:
            try:
                device_id = self.conn.recv(1024).decode("utf-8")
                if device_id == "close":
                    print("Closing connection")
                    self.meta.set_global_info(
                        {
                            SigMFFile.DATATYPE_KEY: self.datatype,
                            SigMFFile.SAMPLE_RATE_KEY: self.sample_rate,
                            SigMFFile.DESCRIPTION_KEY: self.description,
                            SigMFFile.AUTHOR_KEY: self.author,
                            SigMFFile.DATASET_KEY: f"{self.filename}.sigmf-data",
                            SigMFFile.HW_KEY: self.hw,
                            SigMFFile.VERSION_KEY: self.version,
                            SigMFFile.COMMENT_KEY: f"Total number of symbols: {self.total_symbols}",
                        }
                    )

                    self.meta.tofile(self.filename + ".sigmf-meta")
                    self.conn.close()
                    # Stop the execution
                    os._exit(0)
                else:
                    self.update_device_and_create_files(int(device_id))
            except BlockingIOError:
                pass

        # Check if the input is empty
        if len(input_items[0]) == 0:
            return 0
        else:  # Write to file
            print(f"Writing {len(input_items[0])} items to file")
            if self.new_symol:
                self.total_symbols += 1
                self.add_annotation(
                    self.nitems_written,
                    {
                        SigMFFile.ANNOTATION_KEY: {
                            SigMFFile.FREQUENCY_KEY: self.frequency,
                            SigMFFile.DATETIME_KEY: dt.datetime.now().isoformat() + "Z",
                            SigMFFile.COMMENT_KEY: f"LoRa Symbol {self.total_symbols}",
                        }
                    },
                )
            # Write to file
            with open(self.filename + ".sigmf-data", "ab") as f:
                np.array(input_items[0]).tofile(f)
            self.new_symol = False

            self.nitems_written += len(input_items[0])

        return len(input_items[0])
