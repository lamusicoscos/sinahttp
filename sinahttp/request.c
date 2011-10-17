#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "queue.h"
#include "sinahttp.h"
#include "async_sinahttp.h"
#include "request.h"
#include "stat.h"

extern int sinahttp_logging;

struct sinahttp_request *sinahttp_request_new(struct evhttp_request *req, uint64_t id)
{
    struct sinahttp_request *s_req;
    sinahttp_ts start_ts;
    
    sinahttp_ts_get(&start_ts);
    s_req = malloc(sizeof(struct sinahttp_request));
    s_req->req = req;
    s_req->start_ts = start_ts;
    s_req->id = id;
    s_req->async = 0;
    s_req->index = -1;
    TAILQ_INSERT_TAIL(&sinahttp_reqs, s_req, entries);
    
    AS_DEBUG("sinahttp_request_new (%p)\n", s_req);
    
    return s_req;
}

struct sinahttp_request *sinahttp_request_get(struct evhttp_request *req)
{
    struct sinahttp_request *entry;
    
    TAILQ_FOREACH(entry, &sinahttp_reqs, entries) {
        if (req == entry->req) {
            return entry;
        }
    }
    
    return NULL;
}

uint64_t sinahttp_request_id(struct evhttp_request *req)
{
    struct sinahttp_request *entry;
     
    entry = sinahttp_request_get(req);
    
    return entry ? entry->id : 0;
}

struct sinahttp_request *sinahttp_async_check(struct evhttp_request *req)
{
    struct sinahttp_request *entry;
    
    entry = sinahttp_request_get(req);
    if (entry && entry->async) {
        return entry;
    }
    
    return NULL;
}

void sinahttp_async_enable(struct evhttp_request *req)
{
    struct sinahttp_request *entry;
    
    if ((entry = sinahttp_request_get(req)) != NULL) {
        AS_DEBUG("sinahttp_async_enable (%p)\n", req);
        entry->async = 1;
    }
}

void sinahttp_request_finish(struct evhttp_request *req, struct sinahttp_request *s_req)
{
    sinahttp_ts end_ts;
    uint64_t req_time;
    char id_buf[64];
    
    AS_DEBUG("sinahttp_request_finish (%p, %p)\n", req, s_req);
    
    sinahttp_ts_get(&end_ts);
    req_time = sinahttp_ts_diff(s_req->start_ts, end_ts);
    
    if (s_req->index != -1) {
        sinahttp_stats_store(s_req->index, req_time);
    }
    
    if (sinahttp_logging) {
        sprintf(id_buf, "%"PRIu64, s_req->id);
        sinahttp_log("", req, req_time, id_buf, 1);
    }
    
    AS_DEBUG("\n");
    
    TAILQ_REMOVE(&sinahttp_reqs, s_req, entries);
    free(s_req);
}

void sinahttp_async_finish(struct evhttp_request *req)
{
    struct sinahttp_request *entry;
    
    AS_DEBUG("sinahttp_async_finish (%p)\n", req);
    if ((entry = sinahttp_async_check(req))) {
        AS_DEBUG("sinahttp_async_check found (%p)\n", entry);
        sinahttp_request_finish(req, entry);
    }
}

int get_argument_format(struct evkeyvalq *args)
{
    int format_code = json_format;
    char *format = (char *)evhttp_find_header(args, "format");
    if (format && !strncmp(format, "txt", 3)) {
        format_code = txt_format;
    }
    return format_code;
}

int get_int_argument(struct evkeyvalq *args, char *key, int default_value)
{
    char *tmp;
    if (!key) return default_value;
    tmp = (char *)evhttp_find_header(args, (const char *)key);
    if (tmp) {
        return atoi(tmp);
    }
    return default_value;
}

double get_double_argument(struct evkeyvalq *args, char *key, double default_value)
{
    char *tmp;
    if (!key) return default_value;
    tmp = (char *)evhttp_find_header(args, (const char *)key);
    if (tmp) {
        return atof(tmp);
    }
    return default_value;
}

