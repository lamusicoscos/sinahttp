#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <pwd.h>
#include <grp.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fnmatch.h>
#include "queue.h"
#include "sinahttp.h"
#include "stat.h"
#include "request.h"
#include "options.h"

typedef struct cb_entry {
    char *path;
    void (*cb)(struct evhttp_request *, struct evbuffer *,void *);
    void *ctx;
    TAILQ_ENTRY(cb_entry) entries;
} cb_entry;
TAILQ_HEAD(, cb_entry) callbacks;

int sinahttp_logging = 0;
int callback_count = 0;
uint64_t request_count = 0;
struct evhttp *httpd;
struct event pipe_ev;
struct event_config *cfg;
struct event_base *current_base;
struct evhttp_bound_socket *handle;

int help_cb(int *value);

static void ignore_cb(int sig, short what, void *arg)
{
}

void termination_handler(int signum)
{
	fprintf(stdout, "caught signal %d\n", signum);
    //event_loopbreak();
    event_base_loopbreak(current_base);
}

int get_uid(char *user)
{
    int retcode;
    struct passwd *pw;
    
    pw = getpwnam(user);
    if (pw == NULL) {
        retcode = -1;
    } else {
        retcode = (unsigned) pw->pw_uid;
    }
    return retcode;
}

int get_gid(char *group)
{
    int retcode;
    struct group *grent;
    
    grent = getgrnam(group);
    if (grent == NULL) {
        retcode = -1;
    } else {
        retcode = grent->gr_gid;
    }
    return retcode;
}

int get_user_gid(char *user)
{
    int retcode;
    struct passwd *pw;
    
    pw = getpwnam(user);
    if (pw == NULL) {
        retcode = -1;
    } else {
        retcode = pw->pw_gid;
    }
    return retcode;
}

char **sinahttp_callback_names()
{
    char **callback_names;
    char *tmp;
    struct cb_entry *entry;
    int i = 0, len;
    
    callback_names = malloc(callback_count * sizeof(*callback_names));
    TAILQ_FOREACH(entry, &callbacks, entries) {
        len = strlen(entry->path);
        callback_names[i] = malloc(len + 1);
        tmp = entry->path;
        if (tmp[0] == '/') {
            tmp++;
            len--;
        }
        if (tmp[len - 1] == '*') {
            len--;
        }
        memcpy(callback_names[i], tmp, len);
        callback_names[i][len] = '\0';
        i++;
    }
    
    return callback_names;
}

void generic_request_handler(struct evhttp_request *req, void *arg)
{
    int found_cb = 0, i = 0;
    struct cb_entry *entry;
    struct sinahttp_request *s_req;
    struct evbuffer *evb = evbuffer_new();
    
    // fprintf(stderr, "request for %s from %s\n", req->uri, req->remote_host);
    
    request_count++;
    
    s_req = sinahttp_request_new(req, request_count);
    
    TAILQ_FOREACH(entry, &callbacks, entries) {
        if (fnmatch(entry->path, req->uri, FNM_NOESCAPE) == 0) {
            s_req->index = i;
            (*entry->cb)(req, evb, entry->ctx);
            found_cb = 1;
            break;
        }
        i++;
    }
    
    if (!found_cb) {
        evhttp_send_reply(req, HTTP_NOTFOUND, "", evb);
    }
    
    if (sinahttp_async_check(req) == NULL) {
        sinahttp_request_finish(req, s_req);
    }
    
    evbuffer_free(evb);
}

void sinahttp_init()
{
    if (!current_base) {
        //event_init(); move from libevent 1.4.3+ to libevent 2.0.13+
    	cfg = event_config_new();
    	event_config_avoid_method(cfg, "select");
        current_base = event_base_new_with_config(cfg);
        event_config_free(cfg);
    }
    TAILQ_INIT(&callbacks);
    TAILQ_INIT(&sinahttp_reqs);
}

void sinahttp_free()
{
    struct cb_entry *entry;
    
    while ((entry = TAILQ_FIRST(&callbacks))) {
        TAILQ_REMOVE(&callbacks, entry, entries);
        free(entry->path);
        free(entry);
    }
    evhttp_free(httpd);
    sinahttp_stats_destruct();
}

void sinahttp_set_cb(char *path, void (*cb)(struct evhttp_request *, struct evbuffer *, void *), void *ctx)
{
    struct cb_entry *cbPtr;
    
    cbPtr = (cb_entry *)malloc(sizeof(*cbPtr));
    cbPtr->path = strdup(path);
    cbPtr->cb = cb;
    cbPtr->ctx = ctx;
    TAILQ_INSERT_TAIL(&callbacks, cbPtr, entries);
    
    callback_count++;
    
    printf("registering callback for path \"%s\"\n", path);
}

void define_sinahttp_options() {
    option_define_str("address", OPT_OPTIONAL, "0.0.0.0", NULL, NULL, "address to listen on");
    option_define_int("port", OPT_OPTIONAL, 8080, NULL, NULL, "port to listen on");
    option_define_bool("enable_logging", OPT_OPTIONAL, 0, NULL, NULL, "request logging");
    option_define_bool("daemon", OPT_OPTIONAL, 0, NULL, NULL, "daemonize process");
    option_define_str("root", OPT_OPTIONAL, NULL, NULL, NULL, "chdir and run from this directory");
    option_define_str("user", OPT_OPTIONAL, NULL, NULL, NULL, "run as this user");
    option_define_str("group", OPT_OPTIONAL, NULL, NULL, NULL, "run as this group");
}

int sinahttp_listen()
{
    uid_t uid = 0;
    gid_t gid = 0;
    pid_t pid, sid;
    int errno;
    
    char *address = option_get_str("address");
    int port = option_get_int("port");

    int daemon = option_get_int("daemon");
    char *root = option_get_str("root");
    char *user = option_get_str("user");
    char *group = option_get_str("group");
    sinahttp_logging = option_get_int("enable_logging");
    
    if (daemon) {
        pid = fork();
        if (pid < 0) {
        	event_reinit(current_base);
            return 0;
        } else if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        
        umask(0);
        sid = setsid();
        if (sid < 0) {
            return 0;
        }
    }
    
    if (user != NULL) {
        uid = get_uid(user);
        gid = get_user_gid(user);
        if (uid < 0) {
            uid = atoi(user);
            user = NULL;
        }
        if (uid == 0) {
            err(1, "invalid user");
        }
    }
    if (group != NULL) {
        gid = get_gid(group);
        if (gid < 0) {
            gid = atoi(group);
            if (gid == 0) {
                err(1, "invalid group");
            }
        }
    }
    
    if (root != NULL) {
        if (chroot(root) != 0) {
            err(1, strerror(errno));
        }
        if (chdir("/") != 0) {
            err(1, strerror(errno));
        }
    }
    
    if (getuid() == 0) {
        if (user != NULL) {
            if (initgroups(user, (int) gid) != 0) {
                err(1, "initgroups() failed");
            }
        } else {
            if (setgroups(0, NULL) != 0) {
                err(1, "setgroups() failed");
            }
        }
        if (gid != getgid() && setgid(gid) != 0) {
            err(1, "setgid() failed");
        }
        if (setuid(uid) != 0) {
            err(1, "setuid() failed");
        }
    }
    
    signal(SIGINT, termination_handler);
    signal(SIGQUIT, termination_handler);
    signal(SIGTERM, termination_handler);
    
    //signal_set(&pipe_ev, SIGPIPE, ignore_cb, NULL);
    event_assign(&pipe_ev, current_base, SIGPIPE, EV_SIGNAL|EV_PERSIST, ignore_cb, NULL);
    //signal_add(&pipe_ev, NULL);
    event_add(&pipe_ev, NULL);
    
    sinahttp_stats_init();
    
    //httpd = evhttp_start(address, port);
    httpd = evhttp_new(current_base);
    if (!httpd) {
    	fprintf(stderr, "could not create evhttp.\n");
        return 0;
    }
    handle = evhttp_bind_socket_with_handle(httpd, address, (ev_uint16_t) port);
    if (!handle) {
    	fprintf(stderr, "could not bind to %s:%d\n", address, port);
    	return 0;
    }
    printf("listening on %s:%d\n", address, port);
    
    evhttp_set_gencb(httpd, generic_request_handler, NULL);
    return 1;
}

void sinahttp_run()
{
    //event_dispatch();
    event_base_dispatch(current_base);
}

int sinahttp_main()
{
    if (!sinahttp_listen()) {
        return 1;
    }
    sinahttp_run();
    sinahttp_free();
    return 0;
}
