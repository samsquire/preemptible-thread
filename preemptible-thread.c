 #include <pthread.h>
 #include <string.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <errno.h>
 #include <ctype.h>
 #include <time.h>
 #include <math.h>

 #define handle_error_en(en, msg) \
         do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

 #define handle_error(msg) \
         do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct lightweight_thread {
  int thread_num;
  volatile int preempted;
  int num_loops;
  int *limit;
  volatile int *value;
  int *remembered;
  int kernel_thread_num;
  struct lightweight_thread* (*user_function) (struct lightweight_thread*);
};

 struct thread_info {    /* Used as argument to thread_start() */
     pthread_t thread_id;        /* ID returned by pthread_create() */
     int       thread_num;       /* Application-defined thread # */
     char     *argv_string;      /* From command-line argument */
    int lightweight_threads_num;
  struct lightweight_thread *user_threads;
  volatile int running;
 };

struct timer_thread {
  pthread_t thread_id;
  struct thread_info *all_threads;
  int num_threads;
  int lightweight_threads_num;
  int delay;
  volatile int running;
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
  return 0;
}

 /* Thread start function: display address near top of our stack,
    and return upper-cased copy of argv_string. */

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
      for (int i = 0 ; i < tinfo->lightweight_threads_num; i++) {
        tinfo->user_threads[i].preempted = 0;
        
      }
      int previous = -1;
       for (int i = 0 ; i < tinfo->lightweight_threads_num; i++) {
         if (previous != -1) {
           tinfo->user_threads[previous].preempted = 0;
         }
        tinfo->user_threads[i].preempted = 1;
        tinfo->user_threads[i].user_function(&tinfo->user_threads[i]);
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

int
lightweight_thread_function(struct lightweight_thread* m)
{
    
    while (m->preempted != 0) {
      register_loop(0, 0, m, 100000000);
      for (; m->value[0] < m->limit[0]; m->value[0]++) {
        
        sqrt(m->value[0]);
      }
      printf("Kernel thread %d User thread %d ran\n", m->kernel_thread_num, m->thread_num);
      
    }
    
    
    return 0;
}

struct lightweight_thread*
create_lightweight_threads(int kernel_thread_num, int num_threads) {
struct lightweight_thread *lightweight_threads = 
  calloc(num_threads, sizeof(*lightweight_threads));
       if (lightweight_threads == NULL)
           handle_error("calloc lightweight threads");
  for (int i = 0 ; i < num_threads ; i++) {
    lightweight_threads[i].kernel_thread_num = kernel_thread_num;
    lightweight_threads[i].thread_num = i;
    lightweight_threads[i].num_loops = 1;
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
     int s, timer_s, opt, num_threads;
     pthread_attr_t attr;
     pthread_attr_t timer_attr;
     ssize_t stack_size;
     void *res;
     int timer_result;

     /* The "-s" option specifies a stack size for our threads. */

     stack_size = 16384ul;
     num_threads = 5;
   
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
         handle_error("calloc");
    for (int tnum = 0 ; tnum < num_threads; tnum++) {
      tinfo[tnum].running = 1;
    }
   struct timer_thread *timer_info = calloc(1, sizeof(*timer_info));
     timer_info->running = 1;
      timer_info->delay = 10;
     timer_info->num_threads = num_threads;
      
     if (timer_info == NULL)
         handle_error("calloc timer thread");

     /* Create one thread for each command-line argument. */
    timer_info->all_threads = tinfo;
     for (int tnum = 0; tnum < num_threads; tnum++) {
         tinfo[tnum].thread_num = tnum + 1;
         tinfo[tnum].argv_string = argv[0];

        struct lightweight_thread *lightweight_threads = create_lightweight_threads(tnum, num_threads);
        tinfo[tnum].user_threads = lightweight_threads;
        tinfo[tnum].lightweight_threads_num = num_threads;
         /* The pthread_create() call stores the thread ID into
            corresponding element of tinfo[]. */

         s = pthread_create(&tinfo[tnum].thread_id, &attr,
                            &thread_start, &tinfo[tnum]);
         if (s != 0)
             handle_error_en(s, "pthread_create");
     }

   s = pthread_create(&timer_info[0].thread_id, &timer_attr,
                            &timer_thread_start, &timer_info[0]);
         if (s != 0)
             handle_error_en(s, "pthread_create");

     /* Destroy the thread attributes object, since it is no
        longer needed. */

     s = pthread_attr_destroy(&attr);
     if (s != 0)
         handle_error_en(s, "pthread_attr_destroy");
     s = pthread_attr_destroy(&timer_attr);
     if (s != 0)
         handle_error_en(s, "pthread_attr_destroy timer");

     /* Now join with each thread, and display its returned value. */

   s = pthread_join(timer_info->thread_id, &timer_result);
         if (s != 0)
             handle_error_en(s, "pthread_join");

         printf("Joined timer thread");
   

     for (int tnum = 0; tnum < num_threads; tnum++) {
          tinfo[tnum].running = 0;
         s = pthread_join(tinfo[tnum].thread_id, &res);
         if (s != 0)
             handle_error_en(s, "pthread_join");

         printf("Joined with thread %d; returned value was %s\n",
                 tinfo[tnum].thread_num, (char *) res);
         free(res);      /* Free memory allocated by thread */
       for (int user_thread_num = 0 ; user_thread_num < num_threads; user_thread_num++) {
         
           free(tinfo[tnum].user_threads[user_thread_num].remembered);
           free(tinfo[tnum].user_threads[user_thread_num].value);
         free(tinfo[tnum].user_threads[user_thread_num].limit);
         
       }
       free(tinfo[tnum].user_threads);
     }
   
   
   
     free(timer_info);
     free(tinfo);
     exit(EXIT_SUCCESS);
 }
