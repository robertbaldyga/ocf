#
# Copyright(c) 2019-2021 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from ctypes import (
    c_void_p,
    c_int,
    c_uint32,
    c_uint64,
    CFUNCTYPE,
    Structure,
    POINTER,
    byref,
    cast,
)
from enum import IntEnum

from ..ocf import OcfLib
from .data import Data


class IoDir(IntEnum):
    READ = 0
    WRITE = 1


class IoOps(Structure):
    pass

import pdb

class Io(Structure):
    START = CFUNCTYPE(None, c_void_p)
    HANDLE = CFUNCTYPE(None, c_void_p, c_void_p)
    END = CFUNCTYPE(None, c_void_p, c_int)

    _instances_ = {}
    _deleted_ = {}
    _fields_ = [
        ("_addr", c_uint64),
        ("_flags", c_uint64),
        ("_bytes", c_uint32),
        ("_class", c_uint32),
        ("_dir", c_uint32),
        ("_io_queue", c_void_p),
        ("_start", START),
        ("_priv1", c_void_p),
        ("_priv2", c_void_p),
        ("_handle", HANDLE),
        ("_end", END),
    ]

    @classmethod
    def from_pointer(cls, ref):#
        c = cls.from_address(ref)
        if ref != cast(byref(c), c_void_p).value:
            pdb.set_trace()
        cls._instances_[ref] = c
        OcfLib.getInstance().ocf_io_set_cmpl_wrapper(
            byref(c), None, None, c.c_end
        )
        return c

    @classmethod
    def get_instance(cls, ref):
        k = cast(ref, c_void_p).value
        d1 = cls._instances_
        d2 = cls._deleted_
        if k not in d1:
            pdb.set_trace()
        return cls._instances_[cast(ref, c_void_p).value]

    def del_object(self):
        k = cast(byref(self), c_void_p).value
        d1 = type(self)._instances_
        d2 = type(self)._deleted_
        if k in d1:
            d2[k] = d1[k]
            del d1[k]
        else:
            pdb.set_trace()

    def put(self):
        OcfLib.getInstance().ocf_io_put(byref(self))

    def get(self):
        OcfLib.getInstance().ocf_io_get(byref(self))

    @staticmethod
    @END
    def c_end(io, err):
        Io.get_instance(io).end(err)

    @staticmethod
    @START
    def c_start(io):
        Io.get_instance(io).start()

    @staticmethod
    @HANDLE
    def c_handle(io, opaque):
        Io.get_instance(io).handle(opaque)

    def end(self, err):
        try:
            self.callback(err)
        except:  # noqa E722
            pass

        self.put()
        self.del_object()

    def submit(self):
        k = cast(byref(self), c_void_p).value
        d1 = type(self)._instances_
        d2 = type(self)._deleted_
        if k not in d1:
            pdb.set_trace()
        return OcfLib.getInstance().ocf_volume_submit_io(byref(self))

    def submit_sync(self):
        finished = False
        error = 0
        cond = Condition()
        @CFUNCTYPE(c_void_p, c_int)
        def cb(err):
            error = err
            with cond:
                finished = True
                cond.notify_all()

        io.callback = cb 

        io.submit()

        with cond:
            cond.wait_for(lambda: finished)

        return error

    def submit_flush(self):
        return OcfLib.getInstance().ocf_volume_submit_flush(byref(self))

    def submit_discard(self):
        return OcfLib.getInstance().ocf_volume_submit_discard(byref(self))

    def set_data(self, data: Data, offset: int = 0):
        self.data = data
        OcfLib.getInstance().ocf_io_set_data(byref(self), data, offset)


IoOps.SET_DATA = CFUNCTYPE(c_int, POINTER(Io), c_void_p, c_uint32)
IoOps.GET_DATA = CFUNCTYPE(c_void_p, POINTER(Io))

IoOps._fields_ = [("_set_data", IoOps.SET_DATA), ("_get_data", IoOps.GET_DATA)]

lib = OcfLib.getInstance()
lib.ocf_io_set_cmpl_wrapper.argtypes = [POINTER(Io), c_void_p, c_void_p, Io.END]

lib.ocf_core_new_io_wrapper.argtypes = [c_void_p]
lib.ocf_core_new_io_wrapper.restype = c_void_p

lib.ocf_io_set_data.argtypes = [POINTER(Io), c_void_p, c_uint32]
lib.ocf_io_set_data.restype = c_int

lib.ocf_volume_submit_io.argtypes = [POINTER(Io)]
lib.ocf_volume_submit_io.restype = None

lib.ocf_volume_submit_flush.argtypes = [POINTER(Io)] 
lib.ocf_volume_submit_flush.restype = None

lib.ocf_volume_submit_discard.argtypes = [POINTER(Io)] 
lib.ocf_volume_submit_discard.restype = None
