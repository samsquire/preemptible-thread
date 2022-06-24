# preemptible-thread

How to preempt threads in user space?

You set the looping variable to the limit! You can therefore implement an M:N scheduler with this pattern. I've written this multiplexer in Java, C and Rust.

This is a userspace thread scheduler. It switches between green threads very fast, every 10 milliseconds.

These code examples show preempting a hot loop in a green thread in Rust, C and Java.

Rather than introducing an if statement into every iteration of the loop that slows down the loop, we update the loop invariants to cancel the loop. This approach can be used to create cancellable APIs.

# preemtible-thread-evented.c

This is preeemptible threads with threads for IO events. Normally you would use libev or libuv but I thought I could combine my preemptible green threads with IO scheduled on IO threads. 
