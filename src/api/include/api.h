/* Copyright (c) 2014, Artak Khnkoyan <artak.khnkoyan@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * libapi is a cross platform high performance io library written in c. It
 * provides ability to write event driven servers and applications
 * with continous code.
 */

#ifndef API_H_INCLUDED
#define API_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__)
#include "api_linux.h"
#else
#include "api_win.h"
#endif

#include "api_errno.h"

/* Expand if necessary */
#define API_ERRNO_MAP(XX)                                                     \
    XX(OK, "operation completed successfully")                                \
    XX(NOT_PERMITTED, "operation not permitted")                              \
    XX(NOT_FOUND, "not found")                                                \
    XX(IO_ERROR, "i/o error")                                                 \
    XX(BAD_FILE, "bad file descriptor")                                       \
    XX(TEMPORARY_UNAVAILABLE, "resource temporarily unavailable")             \
    XX(NO_MEMORY, "not enough memory")                                        \
    XX(ACCESS_DENIED, "permission denied")                                    \
    XX(FAULT, "bad address in system call argument")                          \
    XX(ALREADY_EXIST, "already exist")                                        \
    XX(NO_DEVICE, "no such device")                                           \
    XX(INVALID_ARGUMENT, "invalid argument")                                  \
    XX(LIMIT, "limit has been reached")                                       \
    XX(TOO_MANY_FILES, "too many open files")                                 \
    XX(NOT_TYPEWRITER, "not a typewriter")                                    \
    XX(NO_SPACE, "no space left on device")                                   \
    XX(TERMINATE, "terminate")

typedef enum {
#define XX(code, _) API_ ## code = API__ ## code,
    API_ERRNO_MAP(XX)
#undef XX
} api_errno_t;


#ifdef _WIN32
#if defined(BUILD_API_SHARED)
    #define API_EXTERN __declspec(dllexport)
#elif defined(USE_API_SHARED)
    #define API_EXTERN __declspec(dllimport)
#else
    #define API_EXTERN
#endif
#elif __GNUC__ >= 4
    #define API_EXTERN __attribute__((visibility("default")))
#else
    #define API_EXTERN
#endif


/*
 * Memory management object for single loop,
 * each api_loop_t has a single api_pool_t.
 *
 * api_pool_t not thread safe
 */
typedef struct api_pool_t api_pool_t;

/*
 * Platform specific event loop
 */
typedef struct api_loop_t api_loop_t;

/*
 * Signaling suppurt.
 * Currently inside single loop was implemented
 */
typedef struct api_event_t {
    api_loop_t* loop;
    uint64_t value;
    void* reserved;
    int error;
} api_event_t;

/*
 * Filters are mechanisms to inject api_stream_t and change its behavior,
 * for example ssl_stream_t is just a filter attached to api_stream_t.
 *
 * usage: attach filter and do regular api_stream_read/api_stream_write calls
 */
typedef struct api_filter_t {
    struct api_filter_t* next;
    struct api_filter_t* prev;
    struct api_stream_t* stream;
    size_t(*on_read)(struct api_filter_t* filter, char* buffer, size_t length);
    size_t(*on_write)(struct api_filter_t* filter, const char* buffer, size_t length);
    void(*on_read_timeout)(struct api_filter_t* filter);
    void(*on_write_timeout)(struct api_filter_t* filter);
    void(*on_error)(struct api_filter_t* filter, int code);
    void(*on_peerclosed)(struct api_filter_t* filter);
    void(*on_closed)(struct api_filter_t* filter);
    void(*on_terminate)(struct api_filter_t* filter);
    void* data;
} api_filter_t;

typedef enum api_stream_type_t {
    STREAM_Memory,
    STREAM_File,
    STREAM_Tcp,
    STREAM_Udp,
    STREAM_Tty,
    STREAM_Pipe
} api_stream_type_t;

/*
 * api_stream_t is an abstraction that encapsulates tcp, udp, file, pipe, tty
 * and memory read/write operations
 */
typedef struct api_stream_t {
#if defined(__linux__)
    struct {
        void(*processor)(struct api_stream_t* stream, int events);
        struct epoll_event e;
        void* reserved[2];
    } os_linux;
#else
    struct {
        void(*processor)(void* e, DWORD transferred, 
                            OVERLAPPED* overlapped, struct api_loop_t* loop);
        OVERLAPPED read;
        OVERLAPPED write;
        void* reserved[2];
    } os_win;
#endif
    fd_t fd;
    api_stream_type_t type;
    union {
        struct {
            uint64_t read_offset;
            uint64_t write_offset;
        } file;
        struct {
            int dummy;
        } memory;
    } impl;
    api_loop_t* loop;
    api_filter_t operations;
    api_filter_t* filter_head;
    api_filter_t* filter_tail;

    /* failure reasons */
    struct {
        unsigned eof : 1;
        unsigned closed : 1;
        unsigned peer_closed : 1;
        unsigned terminated : 1; /* loop was stopped */
        unsigned read_timeout : 1;
        unsigned write_timeout : 1;
        int error;
    } status;

    /* set to non zero for particular operation tmieout */
    uint64_t read_timeout;
    uint64_t write_timeout;

    /* bandwidths automatically calculated as a pair of total amount of bytes
       transferred and total amount of milliseconds elapsed per operation */
    struct {
        uint64_t sent;
        uint64_t period;
    } write_bandwidth;
    struct {
        uint64_t read;
        uint64_t period;
    } read_bandwidth;

    /* internal use only */
    struct {
        char* buffer;
        size_t offset;
        size_t length;
    } unread;
} api_stream_t;

typedef struct api_address_t {
    struct sockaddr_storage address;
    socklen_t length;
} api_address_t;

/* tcp is subclass of api_stream_t with remote address */
typedef struct api_tcp_t {
    api_stream_t stream;
    api_address_t address;
} api_tcp_t;

/* performa tcp bind, listen and conditional connection accepts */
typedef struct api_tcp_listener_t {
#if defined(__linux__)
    struct {
        void(*processor)(struct api_tcp_listener_t* listener, int events);
        struct epoll_event e;
        void* reserved;
        int af;
    } os_linux;
#else
    struct {
        void(*processor)(void* e, DWORD transferred,
                            OVERLAPPED* overlapped, struct api_loop_t* loop);
        OVERLAPPED ovl;
        void* reserved;
        int af;
    } os_win;
#endif
    fd_t fd;
    api_loop_t* loop;
    api_address_t address;
    struct {
        unsigned closed : 1;
        unsigned terminated : 1;
        int error;
    } status;
    int(*on_accept)(struct api_tcp_listener_t* listener, 
                            api_tcp_t* connection);
    void(*on_error)(struct api_tcp_listener_t* listener, int code);
    void(*on_closed)(struct api_tcp_listener_t* listener);
    void(*on_terminate)(struct api_tcp_listener_t* listener);
} api_tcp_listener_t;

typedef struct api_udp_t {
    api_stream_t stream;
} api_udp_t;

typedef struct api_stat_t {
    size_t size;
    time_t date_create;
    time_t date_access;
    time_t date_modified;
} api_stat_t;

typedef struct api_fs_enum_t {
    const char* pattern;
    int enum_dirs;
    int enum_files;
} api_fs_enum_t;

typedef struct api_fs_event_t {
    const char* path;
    const char* pattern;
    int recursive;
} api_fs_event_t;

/*
 * General api callback prototype
 */
typedef void (*api_loop_fn)(api_loop_t* loop, void* arg);

/*
 * if not specified, all api calls with int return types are returning
 * API_OK on success and API_* error code on failure.
 * API_TERMINATE indicates loop was stopping or stopped
 */

/*
 *	api initialization, should be called before any api_* call
 */
API_EXTERN void api_init();


/*
 * The loop must be one in wich caller executes
 */
API_EXTERN api_pool_t* api_pool_default(api_loop_t* loop);

/*
 * Not thread safe, call for loop in wich caller executes
 */
API_EXTERN void* api_alloc(api_pool_t* pool, size_t size);
API_EXTERN void api_free(api_pool_t* pool, size_t size, void* ptr);


/*
 * Starts new api_loop_t in seperate thread, and returns its handle
 * in loop out parameter.
 * Allowed cross loop calls are
 *   api_loop_stop
 *   api_loop_stop_and_wait
 *   api_loop_wait
 *   api_loop_post
 *   api_loop_exec
 */
API_EXTERN int api_loop_start(api_loop_t** loop);

/*
 * Stops specified loop, without wait, the loop parameter can be
 * loop in wich caller executes
 */
API_EXTERN int api_loop_stop(api_loop_t* loop);

/*
 * Stops specified loop, and waits until it stops.
 * current - loop in wich caller executes
 * loop - lopp to be stopped
 *
 * note: current must not be equals to loop
 */
API_EXTERN int api_loop_stop_and_wait(api_loop_t* current, api_loop_t* loop);

/*
 * Wait until loop stops.
 * current - loop in wich caller executes
 * loop - lopp to be stopped
 *
 * note: current must not be equals to loop
 */
API_EXTERN int api_loop_wait(api_loop_t* current, api_loop_t* loop);

/*
 * Create new parallel task and run it in loop.
 * Pass 0 as stack_size for default
 */
API_EXTERN int api_loop_post(api_loop_t* loop,
                            api_loop_fn callback, void* arg,
                            size_t stack_size);

/*
 * Create new parallel task, run it in loop and wait for its completion
 * Pass 0 as stack_size for default
 */
API_EXTERN int api_loop_exec(api_loop_t* current, api_loop_t* loop,
                            api_loop_fn callback, void* arg,
                            size_t stack_size);

/*
 * Call callback with new stack
 */
API_EXTERN int api_loop_call(api_loop_t* loop,
                            api_loop_fn callback,
                            void* arg,
                            size_t stack_size);

/*
 * Convert current thread to loop and run callback inside it
 */
API_EXTERN int api_loop_run(api_loop_fn callback, void* arg, size_t stack_size);

/*
 * Sleep current executing task in specified period of milliseconds
 */
API_EXTERN int api_loop_sleep(api_loop_t* loop, uint64_t period);

/*
 * Sleep current executing task and wake it up when loop is in idle for
 * specified period of milliseconds
 */
API_EXTERN int api_loop_idle(api_loop_t* loop, uint64_t period);


/*
 * Initialize event for signaling
 */
API_EXTERN int api_event_init(api_event_t* ev, api_loop_t* loop);

/*
 * Increment signal count
 */
API_EXTERN int api_event_signal(api_event_t* ev, api_loop_t* loop);

/*
 * Wait for signal
 */
API_EXTERN int api_event_wait(api_event_t* ev, uint64_t timeout);


/*
 * Initialize api_stream_t structure with particular type and handle
 */
API_EXTERN void api_stream_init(api_stream_t* stream,
                                api_stream_type_t type, fd_t fd);

/*
 * Attach stream to loop, after this call any api_stream_* calls should be
 *  made inside that loop
 */
API_EXTERN int api_stream_attach(api_stream_t* stream, api_loop_t* loop);

/*
 * Read from stream.
 * Returns amount of bytes readed or 0 on failure, in this case check
 * stream.status fields for failure reason
 */
API_EXTERN size_t api_stream_read(api_stream_t* stream,
                                    char* buffer, size_t length);

/*
 * Put data back to stream for further read.
 * Helpfull if multiple parsers will be executed in a sequense manner.
 * If there is data already unreaded then it will be overwritten
 */
API_EXTERN size_t api_stream_unread(api_stream_t* stream,
                                    const char* buffer, size_t length);

/*
 * Write to stream.
 * Returns amount of bytes writed.
 * If return value less than length, then call failed, in this case check
 * stream.status fields for failure reason
 */
API_EXTERN size_t api_stream_write(api_stream_t* stream,
                                    const char* buffer, size_t length);

/*
 * Transfers from src to dst.
 * Like a proxy, read and write operations will run in parallel
 */
API_EXTERN int api_stream_transfer(api_stream_t* dst,
                                api_stream_t* src,
                                size_t chunk_size,
                                size_t* transferred);


/*
 * Close stream
 */
API_EXTERN int api_stream_close(api_stream_t* stream);

/*
 * see description of api_filter_t
 */
API_EXTERN void api_filter_attach(api_filter_t* filter, api_stream_t* stream);
API_EXTERN void api_filter_detach(api_filter_t* filter, api_stream_t* stream);


/*
 * Bind and listen for tcp connections
 */
API_EXTERN int api_tcp_listen(api_tcp_listener_t* listener, 
                            api_loop_t* loop,
                            const char* ip, int port, int backlog);

/*
 * Accept tcp connection, conditional accepts can be done by setting
 * on_accept callback to api_tcp_listener_t
 */
API_EXTERN int api_tcp_accept(api_tcp_listener_t* listener, api_tcp_t* tcp);

/*
 * Stop listening for tcp connections and close listener
 */
API_EXTERN int api_tcp_close(api_tcp_listener_t* listener);


/*
 * Init tcp, connect and attach to loop
 */
API_EXTERN int api_tcp_connect(api_tcp_t* tcp,
                                api_loop_t* loop,
                                const char* ip, int port,
                                uint64_t timeout);

/*
 *	udp
 */


/*
 *	tty
 */


/*
 *	pipe
 */


/*
 * Get file info by file path
 */
API_EXTERN int api_fs_stat(const char* path, api_stat_t* stat);

/*
 * Create or override a file as api_stream_t
 */
API_EXTERN int api_fs_create(api_stream_t* stream, const char* path);

/*
 * Open file as api_stream_t
 */
API_EXTERN int api_fs_open(api_stream_t* stream, const char* path);

/*
 * Enumerate files.
 * Not implemented
 */
API_EXTERN int api_fs_enum(api_fs_enum_t* options);

/*
 * Listen for file system changes
 * Not implemented
 */
API_EXTERN int api_fs_listen(api_loop_t* loop, api_fs_event_t* options);


/*
 * Get current time as amount of milliseconds since 1970/1/1 in UTC
 */
API_EXTERN uint64_t api_time_current();


#ifdef __cplusplus
} // extern "C"
#endif

#endif // API_H_INCLUDED