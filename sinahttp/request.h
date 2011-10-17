#ifndef _REQUEST_H
#define _REQUEST_H

struct sinahttp_request {
    struct evhttp_request *req;
    sinahttp_ts start_ts;
    uint64_t id;
    int index;
    int async;
    TAILQ_ENTRY(sinahttp_request) entries;
};
TAILQ_HEAD(, sinahttp_request) sinahttp_reqs;

struct sinahttp_request *sinahttp_request_new(struct evhttp_request *req, uint64_t id);
struct sinahttp_request *sinahttp_request_get(struct evhttp_request *req);
struct sinahttp_request *sinahttp_async_check(struct evhttp_request *req);
void sinahttp_request_finish(struct evhttp_request *req, struct sinahttp_request *s_req);;

#endif
