Heapusage
=========

| **Linux** | **Mac** |
|-----------|---------|
| [![Linux](https://github.com/d99kris/heapusage/workflows/Linux/badge.svg)](https://github.com/d99kris/heapusage/actions?query=workflow%3ALinux) | [![macOS](https://github.com/d99kris/heapusage/workflows/macOS/badge.svg)](https://github.com/d99kris/heapusage/actions?query=workflow%3AmacOS) |

Heapusage is a light-weight tool for finding heap memory errors in Linux and
macOS applications. It provides a small subset of Valgrind's memcheck
functionality, and can be a useful alternative to it for debugging memory
leaks in certain scenarios such as:
- Large complex applications which cannot be run at Valgrind slowdown speed
- Embedded systems with CPU architectures not supported by Valgrind

Like Valgrind, it is recommended to run Heapusage on a debug build of the
application to be analyzed.

While Heapusage has less performance impact than Valgrind, its analysis is
less precise. It may report leaks originating from system libraries (e.g.
libc functions like `printf()`) that might be free'd when the system library
is being cleaned up.

Example Usage
=============

    $ heapusage ./ex001
    ==2933== Heapusage - https://github.com/d99kris/heapusage
    ==2933== 
    ==2933== HEAP SUMMARY:
    ==2933==     in use at exit: 12221 bytes in 4 blocks
    ==2933==   total heap usage: 5 allocs, 1 frees, 13332 bytes allocated
    ==2933==    peak heap usage: 13332 bytes allocated
    ==2933== 
    ==2933== 6666 bytes in 3 block(s) are lost, originally allocated at:
    ==2933==    at 0x00007fd04d062c88: malloc (humain.cpp:154)
    ==2933==    at 0x00005611e856c1a4: main (ex001.c:29)
    ==2933==    at 0x00007fd04ce470b3: __libc_start_main
    ==2933==    at 0x00005611e856c0ae: _start
    ==2933== 
    ==2933== 5555 bytes in 1 block(s) are lost, originally allocated at:
    ==2933==    at 0x00007fd04d062c88: malloc (humain.cpp:154)
    ==2933==    at 0x00005611e856c17f: main (ex001.c:19)
    ==2933==    at 0x00007fd04ce470b3: __libc_start_main
    ==2933==    at 0x00005611e856c0ae: _start
    ==2933== 
    ==2933== LEAK SUMMARY:
    ==2933==    definitely lost: 12221 bytes in 4 blocks
    ==2933== 

Supported Platforms
===================
Heapusage is primarily developed and tested on Linux, but basic
functionality should work in macOS as well. Current version has been tested on:
- macOS Big Sur 11.0
- Ubuntu 20.04 LTS

Limitation: On macOS this tool relies on code injection using
DYLD_INSERT_LIBRARIES, which generally does not work with third-party
applications in a standard system. Using it on (your own) applications built
from source should work fine though.

Installation
============
Pre-requisites (Ubuntu):

    sudo apt install git cmake build-essential

Optional pre-requisite for source filename/line-number in callstacks (Ubuntu):

    sudo apt install binutils-dev

Download the source code:

    git clone https://github.com/d99kris/heapusage && cd heapusage

Build:

    mkdir -p build && cd build && cmake .. && make -s

Optionally install in system:

    sudo make install

Usage
=====
General usage syntax:

    heapusage [-d] [-m minsize] [-n] [-o path] [-t tools] PROG [ARGS..]
    heapusage --help
    heapusage --version

Options:

    -d     debug mode, running program through debugger

    -m <minsize>
           min alloc size to enable analysis for (default 0)

    -n     no symbol lookup (faster)

    -o <path>
           write output to specified file path, instead of stderr

    -t <tools>
           analysis tools to use (default "leak")

    PROG   program to run and analyze

    [ARGS] optional arguments to the program

    --help display this help and exit

    --version
           output version information and exit

Supported tools (for option -t):

    all    enables all supported tools below

    double-free
           detect free'ing of buffers already free'd

    leak   detect memory allocations never free'd

    overflow
           detect buffer overflows, i.e. access beyond allocated memory

    use-after-free
           detect access to free'd memory buffers

Examples:

    heapusage -t leak,overflow -m 2048 ./ex001
           analyze heap allocations of minimum 2048 bytes for leaks and overflows.

    heapusage -t all -m 0 ./ex002
           analyze heap allocations of any size with all tools.

Programs being ran with Heapusage can themselves also request reports from
Heapusage, while the program is running, by using the `hu_report()` public API
function. For doing so, they must include the `huapi.h` public header file, link
with the Heapusage shared library itself and call `hu_report()` when wanted.
Still, this only works if the program is running through the `heapusage` tool.

Output Format
=============
Example output:

    ==2933== Heapusage - https://github.com/d99kris/heapusage
    ==2933== 
    ==2933== HEAP SUMMARY:
    ==2933==     in use at exit: 12221 bytes in 4 blocks
    ==2933==   total heap usage: 5 allocs, 1 frees, 13332 bytes allocated
    ==2933==    peak heap usage: 13332 bytes allocated
    ==2933== 
    ==2933== 6666 bytes in 3 block(s) are lost, originally allocated at:
    ==2933==    at 0x00007fd04d062c88: malloc (humain.cpp:154)
    ==2933==    at 0x00005611e856c1a4: main (ex001.c:29)
    ==2933==    at 0x00007fd04ce470b3: __libc_start_main
    ==2933==    at 0x00005611e856c0ae: _start
    ==2933== 
    ==2933== 5555 bytes in 1 block(s) are lost, originally allocated at:
    ==2933==    at 0x00007fd04d062c88: malloc (humain.cpp:154)
    ==2933==    at 0x00005611e856c17f: main (ex001.c:19)
    ==2933==    at 0x00007fd04ce470b3: __libc_start_main
    ==2933==    at 0x00005611e856c0ae: _start
    ==2933== 
    ==2933== LEAK SUMMARY:
    ==2933==    definitely lost: 12221 bytes in 4 blocks
    ==2933== 

Source code filename and line numbers are only supported on Linux, when package
binutils-dev is available. On macOS one can use atos to determine source code
details.

Technical Details
=================
Heapusage intercepts calls to malloc/free/calloc/realloc and logs each memory
allocation and free. For overflow and use-after-free it uses protected memory
pages using `mprotect()` to detect writing outside valid allocations.

Limitations
===========
Heapusage does currently not intercept calls to:
- aligned_alloc
- malloc_usable_size
- memalign
- posix_memalign
- pvalloc
- valloc

Third-party Libraries
---------------------
Heapusage is implemented in C++. Its source tree includes the source code of the
following third-party libraries:

- [backward-cpp](https://github.com/bombela/backward-cpp) -
  Copyright 2013 Google Inc - [MIT License](/ext/backward-cpp/LICENSE.txt)

Alternatives
============
There are many heap memory debuggers available for Linux and macOS, for
example:

- Address Sanitizer / Leak Sanitizer
- Electric Fence
- Mtrace
- Valgrind

License
=======
Heapusage is distributed under the BSD 3-Clause license. See LICENSE file.

Keywords
========
linux, macos, heap usage, finding memory leaks, alternative to valgrind.
