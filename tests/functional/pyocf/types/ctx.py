#
# Copyright(c) 2019-2021 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from ctypes import c_void_p, Structure, c_char_p, cast, pointer, byref, c_int
import weakref

from .logger import LoggerOps, Logger
from .data import DataOps, Data
from .cleaner import CleanerOps, Cleaner
from .shared import OcfError
from ..ocf import OcfLib
from .queue import Queue
from .volume import RamVolume


class OcfCtxOps(Structure):
    _fields_ = [
        ("data", DataOps),
        ("cleaner", CleanerOps),
        ("logger", LoggerOps),
    ]


class OcfCtxCfg(Structure):
    _fields_ = [("name", c_char_p), ("ops", OcfCtxOps), ("logger_priv", c_void_p)]


class OcfCtx:
    default = None

    def __init__(self, lib, name, logger, data, cleaner):
        self.logger = logger
        self.data = data
        self.cleaner = cleaner
        self.ctx_handle = c_void_p()
        self.lib = lib
        self.volume_types_count = 1
        self.volume_types = {}
        self.caches = []

        self.cfg = OcfCtxCfg(
            name=name,
            ops=OcfCtxOps(
                data=self.data.get_ops(),
                cleaner=self.cleaner.get_ops(),
                logger=logger.get_ops(),
            ),
            logger_priv=cast(pointer(logger.get_priv()), c_void_p),
        )

        result = self.lib.ocf_ctx_create(byref(self.ctx_handle), byref(self.cfg))
        if result != 0:
            raise OcfError("Context initialization failed", result)

        if self.default is None or self.default() is None:
            type(self).default = weakref.ref(self)

    @classmethod
    def with_defaults(cls, logger):
        return cls(
            OcfLib.getInstance(),
            b"PyOCF default ctx",
            logger,
            Data,
            Cleaner,
        )

    @classmethod
    def get_default(cls):
        if cls.default is None or cls.default() is None:
            raise Exception("No context instantiated yet")

        return cls.default()

    def register_volume_type(self, volume_type):
        self.volume_types[self.volume_types_count] = volume_type
        volume_type.type_id = self.volume_types_count

        print(f"ctx.py registering volume {volume_type.__name__}")
        result = self.lib.ocf_ctx_register_volume_type(
            self.ctx_handle,
            self.volume_types_count,
            byref(self.volume_types[self.volume_types_count].get_props()),
        )
        if result != 0:
            raise OcfError("RamVolume type registration failed", result)

        self.volume_types_count += 1

    def unregister_volume_type(self, vol_type):
        if not vol_type.type_id:
            raise Exception("Already unregistered")

        self.lib.ocf_ctx_unregister_volume_type(
            self.ctx_handle, vol_type.type_id
        )

        del self.volume_types[vol_type.type_id]

    def cleanup_volume_types(self):
        for k, vol_type in list(self.volume_types.items()):
            if vol_type:
                self.unregister_volume_type(vol_type)

    def stop_caches(self):
        for cache in self.caches[:]:
            cache.stop()

    def exit(self):
        self.stop_caches()
        self.cleanup_volume_types()

        self.lib.ocf_ctx_put(self.ctx_handle)

        # self.cfg = None
        # self.logger = None
        # self.data = None
        # self.cleaner = None
        # Queue._instances_ = {}
        # RamVolume._instances_ = {}
        # Data._instances_ = {}
        # Logger._instances_ = {}



lib = OcfLib.getInstance()
lib.ocf_mngt_cache_get_by_name.argtypes = [c_void_p, c_void_p, c_void_p]
lib.ocf_mngt_cache_get_by_name.restype = c_int
