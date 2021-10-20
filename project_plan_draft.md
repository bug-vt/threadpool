# Fork-Join Threadpool plan (in progress)

## Thread pool
    - Global Queue for external future submissions
        - Mutex lock for synchronization since it will be shared with all worker
          threads
    - Worker Thread list
    - Completed task list
    - Shutdown boolean flag to broadcast end of all task to all thread

## Worker Thread
    - Deque of pending futures
        - Mutex and conditional variable for synchronization since it will be
          shared with other worker threads
    - Create subtask (Internal submission) and add to its own deque
    - Work stealing approach
        - Perform task dequeuing from its own deque
        - If empty, check global queue first and dequeue
        - Else, steal tasks from the top of other workers' deques
    - Thread local variable (it was mentioned to use for distinguishing between external and
      internal submission, but need further clarification for its use)
    - Work helping for optimization (disscuss below)

## Future
    - Task function pointer that thread would execute 
    - Arguments for task
    - Result 
    - Conditional variable to signal worker thread (also need further
      clarification for its use)   

## Work Helping 
    - Stretegy to minimize sleeping of thread
    - Need further clarification


## Other
    - Starting with single mutex lock
    - More lock can be added later for further optimization

