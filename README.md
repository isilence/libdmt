## dlm

**dlm** is a library for optimised memory transfers between heterogeneous
devices and main memory, that includes GPUs, NICs, etc. It chooses the
most efficient way of data copying considering
DMA, zero-copy technique (e.g. DMA-BUF) and peer-to-peer.
It supports various HPC frameworks and provides distinct memory entity
with a common abstract interface.

### Supported frameworks
- virtual memory storage, vms (RAM)
- OpenCL
- Infiniband (ibverbs)

### Prerequisites
* gcc
* OpenCL 1.2+ SDK
* rdma-core
* CMake 2.6
* cmocka

### Installation

```bash
mkdir build && cd ./build
cmake ../ && make

# run tests
ctest
```

### Example

```C
#include <dlm/providers/vms.h>
#include <dlm/providers/opencl.h>

// initialise
struct dlm_mem_vms *vms;
struct dlm_mem_cl *cl;

vms = dlm_vms_allocate_memory(size);
cl = dlm_cl_allocate_memory(ctx, size, CL_MEM_READ_WRITE);
sz = vms->mem.size; // equal or greater
fill_array((char *)vms->va, sz);

// request memory_copy
struct dlm_mem *dvms = dlm_to_mem(vms);
struct dlm_mem *dcl = dlm_to_mem(cl);

ret = dlm_mem_copy(dvms, dcl);
check(ret);

// memory usage
wait_with_epoll(cl->fd);
check(cl->err);
cl_mem opencl_mem = cl->clmem;
run_opencl(opencl_mem);
```

