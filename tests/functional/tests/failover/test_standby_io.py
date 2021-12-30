import pytest
from datetime import timedelta

from pyocf.types.volume import RamVolume
from pyocf.types.cache import Cache
from pyocf.types.queue import Queue
from pyocf.utils import Size
from pyocf.types.shared import CacheLineSize
from pyocf.types.ctx import OcfCtx
from pyocf.rio import Rio, ReadWrite
from pyocf.helpers import get_collision_segment_page_location, get_collision_segment_size

@pytest.mark.parametrize("cacheline_size", CacheLineSize)
def test_test_standby_io(pyocf_ctx, cacheline_size):
    num_jobs = 8
    qd = 8
    runtime = 30

    vol_size = Size.from_MiB(100)
    cache_vol = RamVolume(vol_size)

    cache = Cache(owner = OcfCtx.get_default(), cache_line_size=cacheline_size)

    cache.start_cache(init_default_io_queue = False)

    for i in range(num_jobs):
        cache.add_io_queue(f"io-queue-{i}")

    cache.standby_attach(cache_vol)

    r = (
            Rio()
            .target(cache)
            .njobs(num_jobs)
            .readwrite(ReadWrite.RANDWRITE)
            .size(vol_size)
            .io_size(Size.from_GiB(100))
            .bs(Size.from_KiB(4))
            .qd(qd)
            .time(timedelta(seconds = runtime))
            .time_based()
            .run(cache.io_queues)
        )
 

@pytest.mark.parametrize("cacheline_size", CacheLineSize)
def test_test_standby_io_metadata(pyocf_ctx, cacheline_size):
    num_jobs = 8
    qd = 1
    runtime = 30

    vol_size = Size.from_MiB(200)
    cache_vol = RamVolume(vol_size)

    cache = Cache(owner = OcfCtx.get_default(), cache_line_size=cacheline_size)

    cache.start_cache(init_default_io_queue = False)

    for i in range(num_jobs):
        cache.add_io_queue(f"io-queue-{i}")

    cache.standby_attach(cache_vol)

    start = get_collision_segment_page_location(cache)
    count = get_collision_segment_size(cache)
    io_offset = Size.from_page(start)
    io_size = Size.from_page(count)

    print(f"{start} {count} <----")

    r = (
            Rio()
            .target(cache)
            .njobs(num_jobs)
            .readwrite(ReadWrite.RANDWRITE)
            .size(io_size)
            .io_size(Size.from_GiB(100))
            .bs(Size.from_KiB(16))
            .offset(io_offset)
            .qd(qd)
            .time(timedelta(seconds = runtime))
            .time_based()
            .norandommap()
            .run(cache.io_queues)
        )
