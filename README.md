# preemptible-thread

[This idea comes from inspiration from Stephen from this Stackoverflow question : how to preempt a thread from userspace](https://stackoverflow.com/questions/72625285/preempt-a-hot-loop-thread-from-user-space)

How to preempt threads in user space?

You set the looping variable to the limit! You can therefore implement an M:N scheduler with this pattern. I've written this multiplexer in Java, C and Rust. See preemtible-thread-evented.c and preemtible-thread.rs and the Java Main.java and Scheduler.java.

This is a userspace thread scheduler. It switches between green threads very fast as frequently as you want it to.

This scheduler cannot preempt arbitrary code, it requires no cooperation at descheduling time - only you register the memory locations of the loop variable.

These code examples show preempting a hot loop in a green thread in Rust, C and Java.

Rather than introducing an if statement into every iteration of the loop that slows down the loop, we update the loop invariants to cancel the loop. This approach can be used to create cancellable APIs.

# preemptible-thread-evented.c

This is preeemptible threads with threads for IO events. Normally you would use libev or libuv but I thought I could combine my preemptible green threads with IO scheduled on IO threads. 

Multiple green threads schedule events to read from the file system and the IO threads handle the blocking reads.

I should probably change IO callbacks to be enqueued onto the RingBuffer as they could currently run at the same time as a lightweight user thread.

# LICENCE

With the exception of the ringbuffer code in -evented.c

BSD Zero Clause License

Copyright (C) 2023 by Samuel Michael Squire sam@samsquire.com

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
