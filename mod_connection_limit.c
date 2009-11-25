/* 
**
**  To play with this connection_limit module first compile it into a
**  DSO file and install it into Apache's modules directory 
**  by running:
**
**    $ apxs -c -i mod_connection_limit.c
**
**  Then activate it in Apache's httpd.conf file for instance
**  for the URL /connection_limit in as follows:
**
**    #   httpd.conf
**    LoadModule connection_limit_module modules/mod_connection_limit.so
**    <Location /connection_limit>
**    SetHandler connection_limit
**    </Location>
**
**  Then after restarting Apache via
**
**    $ apachectl restart
**
**  you immediately can request the URL /connection_limit and watch for the
**  output of this module. This can be achieved for instance via:
**
**    $ lynx -mime_header http://localhost/connection_limit 
**
**  The output should be similar to the following one:
**
**    HTTP/1.1 200 OK
**    Date: Tue, 31 Mar 1998 14:42:22 GMT
**    Server: Apache/1.3.4 (Unix)
**    Connection: close
**    Content-Type: text/html
*/ 


#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"

typedef struct connection_limit_conn_ring connection_limit_conn_ring;
struct connection_limit_conn_ring {
    long unsigned int created_at;
    connection_limit_conn_ring *next;
};

typedef struct connection_limit_server_entry connection_limit_server_entry;
struct connection_limit_server_entry {
    int connection_count;
    int current_connection;
    long unsigned int updated_at;
    connection_limit_server_entry *next;
    server_rec *server;
    connection_limit_conn_ring *ring;
};
connection_limit_server_entry *server_entry;

typedef struct connection_limit_server_config connection_limit_server_config;
struct connection_limit_server_config {
    int connection_enable;
    int connection_update;
    connection_limit_server_entry *server_entry;
};

module AP_MODULE_DECLARE_DATA connection_limit_module;

static int connection_limit_handler(request_rec *r)
{
    
    long unsigned int updated_at = apr_time_sec(apr_time_now());
    connection_limit_server_config *config;
    connection_limit_server_entry *entry;

    if (!strcmp(r->handler, "connection_limit")) {
        r->content_type = "text/html";

        ap_rprintf(r, "<table><tr>");
        ap_rprintf(r, "<th>Vhost</th>");
        ap_rprintf(r, "<th>Count</th>");
        ap_rprintf(r, "<th>Time</th>");
        ap_rprintf(r, "<th>64/Ring</th>");
        ap_rprintf(r, "</tr>");
        for (entry = server_entry; entry; entry = entry->next) {
            config = (connection_limit_server_config *) ap_get_module_config(entry->server->module_config, &connection_limit_module);
            if (!config->connection_enable) {
                continue;
            }

            ap_rprintf(r, "<tr>");
            ap_rprintf(r, "<td><a href='http://%s/'>%s</a></td>", entry->server->server_hostname, entry->server->server_hostname);
            ap_rprintf(r, "<td>%d</td>", entry->connection_count);
            ap_rprintf(r, "<td>%d</td>", config->connection_update);
            ap_rprintf(r, "<td>%lu</td>", updated_at - entry->ring->created_at);
            ap_rprintf(r, "</tr>");
        }
        ap_rprintf(r, "</table>");
        return OK;

    } else {

        config = (connection_limit_server_config *) ap_get_module_config(r->server->module_config, &connection_limit_module);

        if (!config->connection_enable) {
            return DECLINED;
        }

        if (r->content_type
            && !strcmp(r->handler, r->content_type)
            && strcmp(r->handler, "application/x-httpd-php5")
            && strcmp(r->handler, "application/x-httpd-php4")) {
            return DECLINED;
        }

        config->server_entry->ring->created_at = updated_at;
        config->server_entry->ring = config->server_entry->ring->next;
        config->server_entry->connection_count++;

        if ((updated_at - config->server_entry->ring->created_at) <= config->connection_update) {
            return HTTP_SERVICE_UNAVAILABLE;
        } else {
            return DECLINED;
	}

    }

}

static void connection_limit_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(connection_limit_handler, NULL, NULL, APR_HOOK_FIRST);
}

static void * connection_limit_create_server_config(apr_pool_t *p, server_rec *s)
{
    int limit;
    apr_shm_t *shm;
    connection_limit_server_entry *entry;
    connection_limit_conn_ring *ring;

    apr_shm_create(&shm, sizeof(connection_limit_server_entry), NULL, p);
    entry = (connection_limit_server_entry *)apr_shm_baseaddr_get(shm);
    entry->connection_count = 0;
    entry->current_connection = 0;
    entry->updated_at = 0;
    entry->server = s;
    if (server_entry) {
        entry->next = server_entry->next;
        server_entry->next = entry;
    } else {
        server_entry = entry;
    }
    connection_limit_server_config *config;
    config = apr_palloc(p, sizeof(connection_limit_server_config));
    config->connection_enable = 0;
    config->connection_update = 30;
    config->server_entry = entry;

    apr_shm_create(&shm, sizeof(connection_limit_conn_ring), NULL, p);
    entry->ring = (connection_limit_conn_ring *)apr_shm_baseaddr_get(shm);
    entry->ring->next = entry->ring;
    for(limit = 1; limit < 64; limit++){
        apr_shm_create(&shm, sizeof(connection_limit_conn_ring), NULL, p);
        ring = (connection_limit_conn_ring *)apr_shm_baseaddr_get(shm);
        ring->next = entry->ring->next;
        entry->ring->next = ring;
    }

    return config;
}

static const char* set_connection_limit(cmd_parms *cmd, void* cfg, const char* arg)
{
    connection_limit_server_config *config;
    config = (connection_limit_server_config *)ap_get_module_config(cmd->server->module_config, &connection_limit_module);
    return NULL;
}

static const char* set_connection_update(cmd_parms *cmd, void* cfg, const char* arg)
{
    connection_limit_server_config *config;
    config = (connection_limit_server_config *)ap_get_module_config(cmd->server->module_config, &connection_limit_module);
    config->connection_update = atoi((char *)arg);
    return NULL;
}

static const char* set_connection_enable(cmd_parms *cmd, void* cfg, const char* arg)
{
    connection_limit_server_config *config;
    config = (connection_limit_server_config *)ap_get_module_config(cmd->server->module_config, &connection_limit_module);
    config->connection_enable = atoi((char *)arg);
    return NULL;
}

static const command_rec connection_limit_cmds[] = {
    AP_INIT_TAKE1("ConnectionLimit", set_connection_limit, NULL, RSRC_CONF, ""),
    AP_INIT_TAKE1("ConnectionUpdate", set_connection_update, NULL, RSRC_CONF, ""),
    AP_INIT_TAKE1("ConnectionEnable", set_connection_enable, NULL, RSRC_CONF, ""),
    {NULL}
};

module AP_MODULE_DECLARE_DATA connection_limit_module = {
    STANDARD20_MODULE_STUFF, 
    NULL,                                         /* create per-dir    config structures */
    NULL,                                         /* merge  per-dir    config structures */
    connection_limit_create_server_config,        /* create per-server config structures */
    NULL,                                         /* merge  per-server config structures */
    connection_limit_cmds,                        /* table of config file commands       */
    connection_limit_register_hooks               /* register hooks                      */
};

