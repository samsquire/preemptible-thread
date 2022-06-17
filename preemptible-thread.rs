extern crate timer;
extern crate chrono;

use std::sync::Arc;
use std::thread;
use std::sync::atomic::{AtomicI32, Ordering};
use std::{time};

struct LightweightThread {
  thread_num: i32, 
  preempted: AtomicI32,
  num_loops: i32,
  limit: Vec<AtomicI32>,
  value: Vec<AtomicI32>,
  remembered: Vec<AtomicI32>,
  kernel_thread_num: i32,
  lightweight_thread: fn(&mut LightweightThread)
}
fn register_loop(loopindex: usize, initialValue: i32, limit: i32, _thread: &mut LightweightThread) {
  if _thread.remembered[loopindex].load(Ordering::Relaxed) < _thread.limit[loopindex].load(Ordering::Relaxed) {
    _thread.value[loopindex].store( _thread.remembered[loopindex].load(Ordering::Relaxed), Ordering::Relaxed);
    _thread.limit[loopindex].store(limit, Ordering::Relaxed);
  } else {
    _thread.value[loopindex].store(initialValue, Ordering::Relaxed);
    _thread.limit[loopindex].store(limit, Ordering::Relaxed);
  }
  
}
fn lightweight_thread(_thread: &mut LightweightThread) {
  register_loop(0usize, 0, 1000000, _thread);
  while _thread.preempted.load(Ordering::Relaxed) == 1 {
    while _thread.value[0].load(Ordering::Relaxed) < _thread.limit[0].load(Ordering::Relaxed) {
      let i = _thread.value[0].load(Ordering::Relaxed);
      f64::sqrt(i.into());
      _thread.value[0].fetch_add(1, Ordering::Relaxed);
    }
  }
  println!("Kernel thread {} User thread {}", _thread.kernel_thread_num, _thread.thread_num)
}

fn main() {
  println!("Hello, world!");
  let timer = timer::Timer::new();
  static mut threads:Vec<LightweightThread> = Vec::new();
  let mut thread_handles = Vec::new();
  for kernel_thread_num in 1..=5 {
    
    let thread_join_handle = thread::spawn(move || {
      
      
      
      for i in 1..=5 {
        let mut lthread = LightweightThread {
          thread_num: i,
          preempted: AtomicI32::new(0),
          num_loops: 1,
          limit: Vec::new(),
          value: Vec::new(),
          remembered: Vec::new(),
          kernel_thread_num: kernel_thread_num.clone(),
          lightweight_thread: lightweight_thread
        };
        
        lthread.limit.push(AtomicI32::new(-1));
      
        lthread.value.push(AtomicI32::new(-1));
      
        lthread.remembered.push(AtomicI32::new(1));
        
        
        unsafe {
          threads.push(lthread);
        }
      }

      
      
      loop {
        let mut previous:Option<&mut LightweightThread> = None;
        unsafe {
          for (_pos, current_thread) in threads.iter_mut().enumerate() {
            
            if current_thread.kernel_thread_num != kernel_thread_num {
              
              continue;
            }
            if !previous.is_none() {
              previous.unwrap().preempted.store(0, Ordering::Relaxed)
            }
            current_thread.preempted.store(1, Ordering::Relaxed);
            (current_thread.lightweight_thread)(current_thread);
            previous = Some(current_thread);
            // println!("Running")
          }
        }
      } // loop forever
  }); // thread
    
    thread_handles.push(thread_join_handle);
  } // thread generation



 

let timer_handle = thread::spawn(move || {
  
  unsafe {

   loop {
  
  
    for thread in threads.iter() {
      thread.preempted.store(0, Ordering::Relaxed);
    }
    let mut previous:Option<usize> = None;
    for (index,  thread) in threads.iter_mut().enumerate() {
      if !previous.is_none() {
        threads[previous.unwrap()].preempted.store(0, Ordering::Relaxed);
      }
      previous = Some(index);
      for loopindex in 0..thread.num_loops {
        thread.remembered[loopindex as usize].store(thread.value[loopindex as usize].load(Ordering::Relaxed), Ordering::Relaxed);
        thread.value[loopindex as usize].store(thread.limit[loopindex as usize].load(Ordering::Relaxed), Ordering::Relaxed);
        
      }
      thread.preempted.store(1, Ordering::Relaxed);
    }

let ten_millis = time::Duration::from_millis(10);


thread::sleep(ten_millis);
     
  } // loop

} // unsafe
  
}); // end of thread
  timer_handle.join();
   for thread in thread_handles {
    thread.join();
  }

  

}
