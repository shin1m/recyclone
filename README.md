# Recyclone

Recyclone is a C++ implementation of a modified version of David F. Bacon's [Recycler](http://www.research.ibm.com/people/d/dfb/papers/Bacon03Pure.pdf).

# Features

* Concurrent garbage collector
* Threads
* Finalizer
* Monitors (mutexes and condition variables for objects)
* Weak pointers
* Types as objects

# Upsides

* No stop the world
  * The collector only stops one thread at a time for a short period of time.
  * Each thread does not need to wait unless increment/decrement queues are full or the object free list is empty.
* Header only
* Small - less than 2K lines of code.

# Downsides

* High memory overhead - each object has at least 64 bytes of overhead.
* No compaction
* No drop-in replacement for new & delete

# How to

## Define Types

See [test/type.h](test/type.h) and [test/pair.h](test/pair.h) if types are not objects for garbage collection.

See [test/test_type_object.cc](test/test_type_object.cc) if types are objects for garbage collection.

## Use Threads

See [test/thread.h](test/thread.h) and [test/test_threads.cc](test/test_threads.cc).

## Use Finalizer

See [test/thread.h](test/thread.h) and [test/test_finalizer.cc](test/test_finalizer.cc).

## Use Monitors

See [test/test_monitor.cc](test/test_monitor.cc).

## Use Weak Pointers

See [test/test_weak_pointer.cc](test/test_weak_pointer.cc).

# Differences from Recycler

## Increment/Decrement Queues

Each mutator thread has its own increment/decrement queues.

They are single producer single consumer circular queues.

This makes memory synchronization between mutators and collector simple.

## Scanning Stacks

Stacks are scanned conservatively.

Only the pointers that differ from the last epoch are checked as candidates.

Other references are tracked by smart pointers.

## Frequency of Cycle Collection

The cycle collection runs only when an increased number of live objects has exceeded a certain threshold.

The key point is that the list of root candidates is represented as an intrusive doubly linked list.

This avoids the overflow of the list.
And this also allows the decrement phase to remove objects from the list and free them immediately when their reference counts have reached to zero.

Therefore, the cycle collection can be skipped unless the number of live objects increases.

## Scanning Object Graphs

Scanning object graphs is done non-recursively by using a linked list.

## No Scanning Blacks on Each Increment/Decrement Operation

Increment/decrement operations do not recursively scan blacks.

Scanning blacks on every increment/decrement operation turned out to be expensive in the implementation.

The increment/decrement operations are changed to mark just only their target objects as black/purple respectively.

# License

The MIT License (MIT)

Copyright (c) 2021 Shin-ichi MORITA

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
