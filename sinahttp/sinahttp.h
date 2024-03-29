#ifndef _SINAHTTP_H
#define _SINAHTTP_H

#include "queue.h"
#include "options.h"
#include <event.h>
#include <evhttp.h>

#define SINAHTTP_VERSION "0.1.0"
#ifndef DUPE_N_TERMINATE
#define DUPE_N_TERMINATE(buf, len, tmp) \
            tmp = malloc((len) + 1); \
            memcpy(tmp, buf, (len)); \
            tmp[(len)]  = '\0'; \
            buf = tmp;
#endif

#if _POSIX_TIMERS > 0

typedef struct timespec sinahttp_ts;

void sinahttp_ts_get(struct timespec *ts);
unsigned int sinahttp_ts_diff(struct timespec start, struct timespec end);

#else

typedef struct timeval sinahttp_ts;

void sinahttp_ts_get(struct timeval *ts);
unsigned int sinahttp_ts_diff(struct timeval start, struct timeval end);

#endif

struct sinahttp_stats {
    uint64_t requests;
    uint64_t *stats_counts;
    uint64_t *average_requests;
    uint64_t *ninety_five_percents;
    char **stats_labels;
    int callback_count;
};

void sinahttp_init();
int sinahttp_main();
int sinahttp_listen();
void sinahttp_run();
void sinahttp_free();
void sinahttp_set_cb(char *path, void (*cb)(struct evhttp_request *, struct evbuffer *,void *), void *ctx);

uint64_t sinahttp_request_id(struct evhttp_request *req);
void sinahttp_async_enable(struct evhttp_request *req);
void sinahttp_async_finish(struct evhttp_request *req);

void sinahttp_log(const char *host, struct evhttp_request *req, uint64_t req_time, const char *id, int display_post);

struct sinahttp_stats *sinahttp_stats_new();
void sinahttp_stats_get(struct sinahttp_stats *st);
void sinahttp_stats_free(struct sinahttp_stats *st);
uint64_t ninety_five_percent(int64_t *int_array, int length);
char **sinahttp_callback_names();

struct AsyncCallbackGroup;
struct AsyncCallback;

/* start a new callback_group. memory will be freed after a call to 
    release_callback_group or when all the callbacks have been run */
struct AsyncCallbackGroup *new_async_callback_group(struct evhttp_request *req, void (*finished_cb)(struct evhttp_request *, void *), void *finished_cb_arg);
/* create a new AsyncCallback. delegation of memory for this callback 
    will be passed to callback_group */
int new_async_callback(struct AsyncCallbackGroup *callback_group, char *address, int port, char *path, void (*cb)(struct evhttp_request *, void *), void *cb_arg);
struct AsyncCallback *new_async_request(char *address, int port, char *path, void (*cb)(struct evhttp_request *, void *), void *cb_arg);
struct AsyncCallback *new_async_request_with_body(char *address, int port, char *path, char *body, void (*cb)(struct evhttp_request *, void *), void *cb_arg);
void free_async_callback_group(struct AsyncCallbackGroup *callback_group);
void init_async_connection_pool(int enable_request_logging);
void free_async_connection_pool();

enum response_formats {json_format, txt_format};
int get_argument_format(struct evkeyvalq *args);
int get_int_argument(struct evkeyvalq *args, char *key, int default_value);
double get_double_argument(struct evkeyvalq *args, char *key, double default_value);

void define_sinahttp_options();

int sinahttp_parse_url(char *endpoint, size_t endpoint_len, char **address, int *port, char **path);
char *sinahttp_encode_uri(const char *uri);

#endif
