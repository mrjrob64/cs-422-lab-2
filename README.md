## Lab 2 for CSE 422

Group: 
Shawn Fong: f.shawn@wustl.edu
Jeremy Robin: j.i.robin@wustl.edu


We're exploring how to create Demand Paging and Pre-paging for a shared memory allocation in the kernel. Given any user function, We allocate space in the kernel through two ways, on demand when a fault happens or account for it properly ahead of time and fully allocate all the pages ahead of time. Then we delve into how to the pros and cons of each strategy.
