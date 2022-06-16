# preemptible-thread

How to preempt threads in user space?

This is a userspace scheduler that multiplexed N green threads onto M kernel threads.

This is an example of preempting a thread in Java in user space that is in a hot loop and in C.

Rather than introducing an if statement into every iteration of the loop that slows down the loop, we update the loop invariants to cancel the loop. This approach can be used to create cancellable APIs.

