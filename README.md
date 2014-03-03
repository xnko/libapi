# libapi

libapi is a cross platform high performance io library written in C. It
provides ability to write event driven servers and applications
with continous code.

## Preface

Writing event driven code in c is hard, and is harder to maintain.

There are libraries providing high level abstractions in this area,
one of bests is a popular [libuv](https://github.com/joyent/libuv).
But decoupling application logic in various callbacks does not
solve problem a lot.

By doing some research I come across combination of tasks also known
as coroutines or fibers, and epoll/IOCP.

libapi was developed to solve this problem.

## Features

 * cross platform (tested on ubuntu and on windows)
 * high performance (benchmarks will be provided)
 * no locks, no blocking
 * easy to scale
 * easy to develop
 * timeout handling
 * timers and idles
 * bandwidth calculations
 * filters
 * error handling
 * proxy
 * ipv4 and ipv6
 * open source
 * ssl stack with openssl
 * http basic stack
 * keep-alive with timeouts
 * http pipelining
 * https as ssl filter

 for complete examples see [demos](https://github.com/xnko/libapi/tree/master/src/demo/src)

## Architecture

Main concepts are loop, stream and a task.

loop is an abstraction over epoll and IOCP, tht runs on a single 
thread. By starting a new loop new thread will be created. Each loop
has its own pool to alloc/free memory.

stream is an abstraction over file, tcp, utp, pipe, tty and memory.
Currently only file and tcp was implemented.

tasks are execution units with seperate stacks that they can be run
in parallel within a single loop (single thread).
In general scheduler will suspend a task that issued an io operation
and will resume one for which an io operation was completed.
This allows to keep stack variables alive during events in a loop,
and as a result allows writing syncronous code.
A task will be created as a result of calls
api_loop_start, api_loop_post, api_loop_exec, api_loop_run

## Documentation

see [libapi/include/api.h](https://github.com/xnko/libapi/blob/master/src/api/include/api.h)

## Community

[Mailing list](https://groups.google.com/forum/#!forum/libapigroup)

## Problems

### Stack

Creating a task requires also creating stack for it. Creating
large stack is memory consuming, creating small stack will cause
stack overflow. But what is bad that we dont know how much stack
size will be needed for a particular task. By default will be
created 8kb stack for a task. If task needs small stack this is
not justified and memory consumed. 
In case when task needs more stack api_loop_call was designed.

The question is how grow up stack automatically on demand ?
