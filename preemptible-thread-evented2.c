#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <stdatomic.h>

#define READ 0
#define WRITE 1
#define PENDING_IO_SIZE 100

 #define handle_error_en(en, msg) \
         do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

 #define handle_error(msg) \
         do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct ringbuffer {
  volatile int last_head;
  volatile int last_tail;
  volatile int head;
  volatile int tail;
  
  int num_io_operations;
  int size;
  struct io_operation **operations;
  int stopped;
};

struct lightweight_thread;
struct thread_info;
struct lightweight_thread {
  int thread_num;
  volatile atomic_int preempted;
  int num_loops;
  int *limit;
  volatile int *value;
  int *remembered;
  int kernel_thread_num;
  struct lightweight_thread* (*user_function) (struct thread_info*, struct lightweight_thread*);
  struct io_thread* io_threads;
  int num_io_threads;
  volatile int tail;
  volatile int head;
  struct ringbuffer* ringbuffer;
  struct thread_info *all_producers;
  int size;
};

struct thread_info {    /* Used as argument to thread_start() */
     pthread_t thread_id;        /* ID returned by pthread_create() */
     int       thread_num;       /* Application-defined thread # */
     char     *argv_string;      /* From command-line argument */
    int lightweight_threads_num;
  struct lightweight_thread *user_threads;
  volatile int running;
 };


struct io_operation {
  int bytes_returned;
  int fd;
  void (*callback) (struct io_operation*, struct ringbuffer*);
  int type;
  int buffer_size;
  char *buffer;


};

struct io_thread {
  int thread_num;

  pthread_t thread_id;
  struct ringbuffer* ringbuffer;
  int running;
  int type;
  volatile int tail;
  volatile int head;
  int num_threads;
  int num_io_threads;
  struct io_thread *threads;
  struct thread_info* all_producers;

};

void
	push(struct lightweight_thread *io_thread, struct io_operation *ptr)
	{
		struct lightweight_thread *tp = io_thread;
    struct ringbuffer *rb = tp->ringbuffer;
		/*
		 * Request next place to push.
		 *
		 * Second assignemnt is atomic only for head shift, so there is
		 * a time window in which thr_p_[tid].head = ULONG_MAX, and
		 * head could be shifted significantly by other threads,
		 * so pop() will set last_head_ to head.
		 * After that thr_p_[tid].head is setted to old head value
		 * (which is stored in local CPU register) and written by @ptr.
		 *
		 * First assignment guaranties that pop() sees values for
		 * head and thr_p_[tid].head not greater that they will be
		 * after the second assignment with head shift.
		 *
		 * Loads and stores are not reordered with locked instructions,
		 * se we don't need a memory barrier here.
		 */
		tp->head = rb->head;
    
		tp->head = __atomic_fetch_add(&rb->head, 1, __ATOMIC_SEQ_CST);
    tp->head = tp->head % io_thread->size;
    rb->head = rb->head % io_thread->size;

		/*
		 * We do not know when a consumer uses the pop()'ed pointer,
		 * se we can not overwrite it and have to wait the lowest tail.
		 */
		while (__builtin_expect(tp->head >= (rb->last_tail + rb->size), 0))
		{
		  printf("Blocking during push..\n");
      
			int min = rb->tail;
      // printf("%d min %d tail %d last tail\n", min, tp->head, rb->last_tail);

			// Update the last_tail_.
			for (size_t i = 0; i < io_thread->num_io_threads; ++i) {
				int tmp_t = io_thread->io_threads[i].tail;

				// Force compiler to use tmp_h exactly once.
				asm volatile("" ::: "memory");

				if (tmp_t < min) {
					min = tmp_t;
        }
			}
      for (int i = 0 ; i < io_thread->thread_num ; i++) {
        for (int j = 0 ; j < io_thread->all_producers[i].lightweight_threads_num; j++) {
          int tmp_t = io_thread->all_producers[i].user_threads[j].tail;

				// Force compiler to use tmp_h exactly once.
				asm volatile("" ::: "memory");

				if (tmp_t < min) {
					min = tmp_t;
        }
        }
      }
			rb->last_tail = min;

			if (tp->head < (rb->last_tail + rb->size)) {
        
				break;
		    }
			
		}
    // free(&tp->operations[tp->head]);
    // printf("Pushing operation\n");

    rb->operations[tp->head] = ptr;
    // printf("Successfully pushed");
		// Allow consumers eat the item.
		tp->head = INT_MAX;
	}

// def pop
struct io_operation*
pop(struct io_thread* io_thread)
	{
		
		struct io_thread *tp = io_thread;
    struct ringbuffer *rb = io_thread->ringbuffer;
		/*
		 * Request next place from which to pop.
		 * See comments for push().
		 *
		 * Loads and stores are not reordered with locked instructions,
		 * se we don't need a memory barrier here.
		 */
		tp->tail = rb->tail;
    // printf("Got tail \n");
		tp->tail = __atomic_fetch_add(&rb->tail, 1, __ATOMIC_SEQ_CST);
    tp->tail = tp->tail % io_thread->ringbuffer->size;
    rb->tail = rb->tail % io_thread->ringbuffer->size;
		/*
		 * tid'th place in ptr_array_ is reserved by the thread -
		 * this place shall never be rewritten by push() and
		 * last_tail_ at push() is a guarantee.
		 * last_head_ guaraties that no any consumer eats the item
		 * before producer reserved the position writes to it.
		 */
		while (__builtin_expect(tp->tail >= (rb->last_head), 0))
		{
      // printf("Blocking during pop %d %d..\n", tp->tail, rb->last_head);
			int min = rb->head;
    // printf("%d min %d tail %d last head\n", min, io_thread->tail, rb->last_head);
			// Update the last_head_.
			for (int i = 0; i < io_thread->num_io_threads; ++i) {
        
				int tmp_h = io_thread->threads[i].head;
      
				// Force compiler to use tmp_h exactly once.
				asm volatile("" ::: "memory");

				if (tmp_h < min) {
					min = tmp_h;
          }
			}
      for (int i = 0 ; i < io_thread->thread_num ; i++) {
        for (int j = 0 ; j < io_thread->all_producers[i].lightweight_threads_num; j++) {
          int tmp_t = io_thread->all_producers[i].user_threads[j].head;

				// Force compiler to use tmp_h exactly once.
				asm volatile("" ::: "memory");

				if (tmp_t < min) {
					min = tmp_t;
        }
        }
      }
			rb->last_head = min;

			if (tp->tail < (rb->last_head)) {
        
				break;
        	}

			if (rb->stopped) { return -1; }

		}
		struct io_operation *ret = (tp->ringbuffer->operations[tp->tail]);
		printf("Returning popped item %d\n", tp->tail);
		// Allow producers rewrite the slot.
		tp->tail = INT_MAX;
		return ret;
	}






struct timer_thread {
  pthread_t thread_id;
  struct thread_info *all_threads;
  int num_threads;
  int lightweight_threads_num;
  int delay;
  volatile int running;
  struct ringbuffer *ringbuffer;
};

static void *
timer_thread_start(void *arg) {
  int iterations = 0;
  struct timer_thread *timer_thread = arg;
  int msec = 0, trigger = timer_thread->delay; /* 10ms */
clock_t before = clock();
while (timer_thread->running == 1 && iterations < 100000) {
do {
  for (int i = 0 ; i < timer_thread->num_threads; i++) {
    for (int j = 0 ; j < timer_thread->all_threads[i].lightweight_threads_num; j++) {
      // printf("Preempting kernel thread %d user thread %d\n", i, j);
      timer_thread->all_threads[i].user_threads[j].preempted = 0;
      
    }
  }
  for (int i = 0 ; i < timer_thread->num_threads; i++) {
    for (int j = 0 ; j < timer_thread->all_threads[i].lightweight_threads_num; j++) {
      
      
      // printf("Preempting kernel thread %d user thread %d\n", i, j);
      for (int loop = 0; loop < timer_thread->all_threads[i].user_threads[j].num_loops; loop++) {
        timer_thread->all_threads[i].user_threads[j].remembered[loop] = timer_thread->all_threads[i].user_threads[j].value[loop];
        timer_thread->all_threads[i].user_threads[j].value[loop] = timer_thread->all_threads[i].user_threads[j].limit[loop];
      }
      
    }
  }

  clock_t difference = clock() - before;
  msec = difference * 1000 / CLOCKS_PER_SEC;
  iterations++;
} while ( msec < trigger && iterations < 100000 );

// printf("Time taken %d seconds %d milliseconds (%d iterations)\n",
//  msec/1000, msec%1000, iterations);
  }
  // stop all lightweight functions
  /*for (int i = 0 ; i < timer_thread->num_threads; i++) {
    for (int j = 0 ; j < timer_thread->all_threads[i].lightweight_threads_num; j++) {
      // printf("Preempting kernel thread %d user thread %d\n", i, j);
      timer_thread->all_threads[i].user_threads[j].preempted = 0;
      
    }
  }*/
   
  return 0;
}

 /* Thread start function: display address near top of our stack,
    and return upper-cased copy of argv_string. */
// def thread_start
static void *
thread_start(void *arg)
 {
     struct thread_info *tinfo = arg;
     char *uargv;

     printf("Thread %d: top of stack near %p; argv_string=%s\n",
             tinfo->thread_num, (void *) &tinfo, tinfo->argv_string);

     uargv = strdup(tinfo->argv_string);
     if (uargv == NULL)
         handle_error("strdup");

     for (char *p = uargv; *p != '\0'; p++) {
         *p = toupper(*p);
       
       }
	
    while (tinfo->running == 1) {
	 // printf("Running kernel thread...\n");
      for (int i = 0 ; i < tinfo->lightweight_threads_num; i++) {
        tinfo->user_threads[i].preempted = 0;
        
      }
      int previous = -1;
       for (int i = 0 ; i < tinfo->lightweight_threads_num; i++) {
         if (previous != -1) {
           tinfo->user_threads[previous].preempted = 0;
         }
        tinfo->user_threads[i].preempted = tinfo->running;
        tinfo->user_threads[i].user_function(tinfo, &tinfo->user_threads[i]);
        previous = i;
      } 
    }
     return uargv;
 }

void
register_loop(int index, int value, struct lightweight_thread* m, int limit) {
  if (m->remembered[index] == -1) {
    m->limit[index] = limit;
    m->value[index] = value;
  } else {
    m->limit[index] = limit;
    m->value[index] = m->remembered[index];
  }
}

// def io_thread()
int
io_thread(void *arg) {
  struct io_thread *io_thread = arg;
  while (io_thread->running == 1) {
    printf("IO thread running\n");
    struct io_operation *io_operation = pop(io_thread);
	if (io_operation == -1) {
		io_thread->running = 0;
		break;
	}
    printf("Processing popped item\n");
    if (io_operation->type == WRITE) {
      
    }
    if (io_operation->type == READ) {
      printf("Operation type is a READ\n");
      // io_operation->bytes_returned = fread(&io_operation->buffer, sizeof(char), io_operation->buffer_size, io_operation->fd);
	  // printf("File no is %d\n", fileno(io_operation->fd));
	  printf("Buffer size is %d\n", io_operation->buffer_size);
       io_operation->bytes_returned = read(io_operation->fd, &io_operation->buffer, io_operation->buffer_size);
	  printf("Read %d bytes\n", io_operation->bytes_returned);
       if (!io_operation->bytes_returned || io_operation->bytes_returned == -1) {
         handle_error("read io thread");
       }
      
      (io_operation->callback)(io_operation, io_thread->ringbuffer);
    }
  }
  return 0;
}

void
do_io(struct io_operation* io_operation, struct ringbuffer* ringbuffer) {
  // printf("IO Callback\n");
  printf("BUFFER %s\n", &io_operation->buffer);
  // printf("Printed buffer\n");
  // free(io_operation->buffer);
  // printf("Freed buffer\n");
  printf("Need to free %d\n", io_operation->fd);
  int close_result = close(io_operation->fd);
  if (close_result == -1) {
	handle_error("close failure");	
  }
  // printf("Successfully closed file\n");
  free(io_operation);
}

// def lightweight_thread
void*
lightweight_thread_function(struct thread_info* t, struct lightweight_thread* m)
{
    
    while (m->preempted == 1) {
      register_loop(0, 0, m, 10000000);
	  // printf("Lightweight function running...%d\n", m->preempted);	
      for (; m->value[0] < m->limit[0]; m->value[0]++) {
      	// printf("Doing work\n");  
        sqrt(m->value[0]);
        char *buffer = (char*)malloc(1024 * sizeof(char));
        memset(buffer, '\0', 1024 * sizeof(char));
        // printf("Allocated buffer\n");
        if (buffer == NULL) {
          handle_error("buffer calloc");
        }
        struct io_operation *operations = calloc(1, sizeof(struct io_operation));
        if (!operations) {
          handle_error("could not allocate operations");
        }
		if (operations == NULL) {
		 handle_error("green thread calloc operations");
		}
        operations->type = READ;
        operations->buffer = buffer;
        
        operations->buffer_size = 1024;
        // printf("Opening file\n");
		
        operations->fd = open("data", O_RDONLY);
       	if (operations->fd == NULL || operations->fd == -1 || operations->fd == 0) {
          // handle_error("could not open file for reading");
		 // printf("Freeing operations");
		 free(operations);	
		 // printf("Freeing buffer");
		 free(buffer);
		 break;
		}
		// printf("%p is pointer to *FILE\n", operations->fd);
        operations->callback = do_io;
        // printf("Pushing read operation\n");
        push(m, operations);
      }
      // printf("Kernel thread %d User thread %d ran\n", m->kernel_thread_num, m->thread_num);
      
    }
    
    
    return 0;
}

struct lightweight_thread*
create_lightweight_threads(int kernel_thread_num, int num_threads, struct ringbuffer* ringbuffer, struct io_thread* io_threads, int size, struct thread_info* tinfo) {
struct lightweight_thread *lightweight_threads = 
  calloc(num_threads, sizeof(struct lightweight_thread));
       if (lightweight_threads == NULL)
           handle_error("calloc lightweight threads");
  for (int i = 0 ; i < num_threads ; i++) {
    lightweight_threads[i].kernel_thread_num = kernel_thread_num;
    lightweight_threads[i].preempted = 1;
    lightweight_threads[i].thread_num = i;
    lightweight_threads[i].num_loops = 1;
    lightweight_threads[i].all_producers = tinfo; // TODO &tinfo
    lightweight_threads[i].ringbuffer = ringbuffer;
    lightweight_threads[i].io_threads = io_threads;
    lightweight_threads[i].num_io_threads = num_threads;
    lightweight_threads[i].head = INT_MAX;
    lightweight_threads[i].tail = INT_MAX;

    lightweight_threads[i].size = size;
    lightweight_threads[i].user_function = lightweight_thread_function;
    int *remembered = calloc(lightweight_threads[i].num_loops, sizeof(*remembered));
    int *value = calloc(lightweight_threads[i].num_loops, sizeof(*value));
    int *limit = calloc(lightweight_threads[i].num_loops, sizeof(*limit));
    lightweight_threads[i].remembered = remembered;
    lightweight_threads[i].value = value;
    lightweight_threads[i].limit = limit;
    for (int j = 0 ; j < lightweight_threads[i].num_loops ; j++) {
    lightweight_threads[i].remembered[j] = -1;
      }
  }
    return lightweight_threads;
}

 int
 main(int argc, char *argv[])
 {
     int io_threads = 5;
     int s, timer_s, io_thread_s, opt, num_threads;
     pthread_attr_t attr;
     pthread_attr_t timer_attr;
     pthread_attr_t io_thread_attr;
     ssize_t stack_size;
     void *res;
     int timer_result;

     /* The "-s" option specifies a stack size for our threads. */

     stack_size = 16384ul;
     num_threads = 5;
	 int number_threads = num_threads;
   
     while ((opt = getopt(argc, argv, "t:")) != -1) {
         switch (opt) {
         
        case 't':
             num_threads = strtoul(optarg, NULL, 0);
             break;
           
         default:
             fprintf(stderr, "Usage: %s [-t thread-size] arg...\n",
                     argv[0]);
             exit(EXIT_FAILURE);
         }
     }

     

     /* Initialize thread creation attributes. */

     s = pthread_attr_init(&attr);
     if (s != 0)
         handle_error_en(s, "pthread_attr_init");
     timer_s = pthread_attr_init(&timer_attr);
     io_thread_s = pthread_attr_init(&io_thread_attr);
     if (timer_s != 0)
         handle_error_en(s, "pthread_attr_init timer_s");

   
     if (stack_size > 0) {
         s = pthread_attr_setstacksize(&attr, stack_size);
         int t = pthread_attr_setstacksize(&timer_attr, stack_size);
       if (t != 0)
             handle_error_en(t, "pthread_attr_setstacksize timer");
         if (s != 0)
             handle_error_en(s, "pthread_attr_setstacksize");
     }

     /* Allocate memory for pthread_create() arguments. */

     struct thread_info *tinfo = calloc(num_threads, sizeof(*tinfo));
     if (tinfo == NULL)
         handle_error("calloc tinfo");
    for (int tnum = 0 ; tnum < num_threads; tnum++) {
      tinfo[tnum].running++;
    }

struct ringbuffer *io_ringbuffer = calloc(1, sizeof(struct ringbuffer));
   io_ringbuffer->size = PENDING_IO_SIZE;
   io_ringbuffer->last_head = -1;
   io_ringbuffer->last_tail = -1;
   io_ringbuffer->tail = 0;
   io_ringbuffer->head = 0;
   struct io_operation **operations = calloc(100, sizeof(*operations));
   io_ringbuffer->operations = operations;
     if (operations == NULL) {
         handle_error("calloc operations");
     }
struct io_thread *io_thread_info = calloc(io_threads, sizeof(struct io_thread));
   printf("Allocated io threads \n");
     if (io_thread_info == NULL)
         handle_error("calloc io_thread_info");
    for (int tnum = 0 ; tnum < io_threads; tnum++) {
      printf("Setting up io thread %d\n", tnum);
      io_thread_info[tnum].running = 1;
      io_thread_info[tnum].threads = io_thread_info;
      io_thread_info[tnum].all_producers = tinfo;
      io_thread_info[tnum].head = INT_MAX;
      io_thread_info[tnum].tail = INT_MAX;
      io_thread_info[tnum].ringbuffer = io_ringbuffer;
      io_thread_info[tnum].num_io_threads = io_threads;
      io_thread_info[tnum].thread_num = tnum;
     
    }
   printf("Set up io_thread_info\n");


   
   struct timer_thread *timer_info = calloc(1, sizeof(struct timer_thread));
     timer_info->running = 1;
     timer_info->delay = 10;
	 timer_info->ringbuffer = io_ringbuffer;
     timer_info->num_threads = num_threads;
      
     if (timer_info == NULL)
         handle_error("calloc timer thread");

     /* Create one thread for each command-line argument. */
    timer_info->all_threads = tinfo;

    
   
   
     for (int tnum = 0; tnum < num_threads; tnum++) {
         tinfo[tnum].thread_num = tnum + 1;
         tinfo[tnum].argv_string = argv[0];

        struct lightweight_thread *lightweight_threads = create_lightweight_threads(tnum, num_threads, io_ringbuffer, io_thread_info, PENDING_IO_SIZE, tinfo);
        tinfo[tnum].user_threads = lightweight_threads;
        tinfo[tnum].lightweight_threads_num = num_threads;
       
         /* The pthread_create() call stores the thread ID into
            corresponding element of tinfo[]. */
       }
   for (int tnum = 0; tnum < num_threads; tnum++) {
		 printf("Creating kernel thread...\n");
         s = pthread_create(&tinfo[tnum].thread_id, &attr,
                            &thread_start, &tinfo[tnum]);
         if (s != 0)
             handle_error_en(s, "tinfo pthread_create");
		
     }
   for (int tnum = 0 ; tnum < io_threads ; tnum++) {
      s = pthread_create(&io_thread_info[tnum].thread_id, &io_thread_attr,
                            &io_thread, &io_thread_info[tnum]);
      printf("Creating io thread\n");
         if (s != 0) {
             handle_error_en(s, "io pthread_create");
     }
    }

   s = pthread_create(&timer_info[0].thread_id, &timer_attr,
                            &timer_thread_start, &timer_info[0]);
         if (s != 0)
             handle_error_en(s, "timer thread pthread_create");

     /* Destroy the thread attributes object, since it is no
        longer needed. */

  


     /* Now join with each thread, and display its returned value. */

	 s = pthread_join(timer_info->thread_id, &timer_result);
         if (s != 0)
             handle_error_en(s, "pthread_join");

	 printf("Joined timer thread\n");
	 io_ringbuffer->stopped = 1;
   
	 printf("Stopping user threads\n");
	 for (int i = 0 ; i < number_threads; i++) {
		tinfo[i].running = 0;
		
		for (int j = 0 ; j < tinfo[i].lightweight_threads_num; j++) {
			tinfo[i].user_threads[j].preempted = 0; // stop the lightweight thread	
			printf("Stopping lightweight thread\n");
			for (int loop = 0 ; loop < tinfo[i].user_threads[j].num_loops; loop++) {
				printf("Stopping loop\n");
				tinfo[i].user_threads[j].value[loop] = tinfo[i].user_threads[j].limit[loop];
			}
		}
	 } 

	 printf("Stopping kernel threads...\n");	
	 printf("Num threads %d", number_threads);	
     for (int tnum = 0; tnum < number_threads; tnum++) {
         tinfo[tnum].running = 0;
		 void* thread_result;
		 printf("Waiting for kernel thread to end...\n"); 
         s = pthread_join(tinfo[tnum].thread_id, &thread_result);
         if (s != 0)
             handle_error_en(s, "pthread_join");

          printf("Joined with thread %d; returned value was %s\n",
                 tinfo[tnum].thread_num, (char *) thread_result);
         // free(res);      
		  // free(tinfo[tnum].argv_string);
		
       for (int user_thread_num = 0 ; user_thread_num < number_threads; user_thread_num++) {
         free(tinfo[tnum].user_threads[user_thread_num].remembered);
         free(tinfo[tnum].user_threads[user_thread_num].value);
         free(tinfo[tnum].user_threads[user_thread_num].limit);
         
       }
       free(tinfo[tnum].user_threads);
     }

    for (int i = 0 ; i < io_threads; i++) {
        void* io_thread_result;
		
        s = pthread_join(io_thread_info[i].thread_id, &io_thread_result);
         if (s != 0)
             handle_error_en(s, "io thread pthread_join");

         printf("Joined io thread\n");
		
   }
     s = pthread_attr_destroy(&attr);
     if (s != 0)
         handle_error_en(s, "pthread_attr_destroy");
     s = pthread_attr_destroy(&timer_attr);
     if (s != 0)
         handle_error_en(s, "pthread_attr_destroy timer");
     s = pthread_attr_destroy(&io_thread_attr);
     if (s != 0)
         handle_error_en(s, "pthread_attr_destroy io thread");
   
     free(operations); 
     free(io_thread_info);
     free(io_ringbuffer);
     free(timer_info);
     free(tinfo);
     exit(EXIT_SUCCESS);
 }
