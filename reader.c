/*
 * Toy mod_cluster structures reader
 *
 * Usage: ./reader  HTTPD_DIR/cache/mod_cluster/manager.node.nodes.slotmem
 *
 * @author Michal Karm Babacek
 */

#include <stdio.h>
#include "apr_time.h"
#include "slotmem.h"
#include "node.h"
#include "balancer.h"
#include "context.h"
#include "domain.h"
#include "host.h"

#define NODE_TABLE     "node.nodes.slotmem"
#define BALANCER_TABLE "balancer.balancers.slotmem"
#define CONTEXT_TABLE  "context.contexts.slotmem"
#define DOMAIN_TABLE   "domain.domain.slotmem"
#define HOST_TABLE     "manager.host.hosts.slotmem"

// Magic constant :-( Got from Hexa editor Bless
#define OFFSET_NODE_TABLE       88
#define OFFSET_BALANCER_TABLE   88
#define OFFSET_DOMAIN_TABLE     88
#define OFFSET_HOST_TABLE       88
#define OFFSET_CONTEXT_TABLE    2008

void print_node_info(nodeinfo_t record, char* date) {
        printf("mess.balancer: %s\n", record.mess.balancer);
        printf("mess.JVMRoute: %s\n", record.mess.JVMRoute);
        printf("mess.Domain: %s\n", record.mess.Domain);
        printf("mess.Host: %s\n", record.mess.Host);
        printf("mess.Port: %s\n", record.mess.Port);
        printf("mess.Type: %s\n", record.mess.Type);
        printf("mess.reversed: %d\n", record.mess.reversed);
        printf("mess.remove: %d\n", record.mess.remove);
        printf("mess.flushpackets: %d\n", record.mess.flushpackets);
        printf("mess.flushwait: %d\n", record.mess.flushwait);
        printf("mess.ping: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.ping));
        printf("mess.smax: %d\n", record.mess.smax);
        printf("mess.ttl: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.ttl));
        printf("mess.timeout: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.timeout));
        printf("mess.id: %d\n", record.mess.id);
        apr_rfc822_date(date,record.mess.updatetimelb);
        printf("mess.updatetimelb: %s\n",date);
        printf("mess.num_failure_idle: %d\n", record.mess.num_failure_idle);
        printf("mess.oldelected: %d\n", record.mess.oldelected);
        printf("mess.lastcleantry: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.lastcleantry));
        apr_rfc822_date(date,record.updatetime);
        printf("updatetime: %s\n",date);
        printf("offset: %d\n", record.offset);
        printf("stat: %s\n", record.stat);
        printf("----------\n");
}

void print_balancer_info(balancerinfo_t record, char* date) {
        printf("balancer: %s\n", record.balancer);
        printf("StickySession: %d\n", record.StickySession);
        printf("StickySessionCookie: %s\n", record.StickySessionCookie);
        printf("StickySessionPath: %s\n", record.StickySessionPath);
        printf("StickySessionRemove: %d\n", record.StickySessionRemove);
        printf("StickySessionForce: %d\n", record.StickySessionForce);
        printf("Timeout: %d\n", record.Timeout);
        printf("Maxattempts: %d\n", record.Maxattempts);
        //apr_rfc822_date(date,record.updatetime);
        //printf("updatetime: %s\n",date);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record.updatetime));
        printf("id: %d\n", record.id);
        printf("----------\n");
}

void print_context_info(contextinfo_t record, char* date) {
        printf("context: %s\n", record.context);
        printf("vhost: %d\n", record.vhost);
        printf("node: %d\n", record.node);
        printf("status: %d\n", record.status);
        printf("nbrequests: %d\n", record.nbrequests);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record.updatetime));
        printf("id: %d\n", record.id);
        printf("----------\n");
}

void print_domain_info(domaininfo_t record, char* date) {
        printf("domain: %s\n", record.domain);
        printf("JVMRoute: %s\n", record.JVMRoute);
        printf("balancer: %s\n", record.balancer);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record.updatetime));
        printf("id: %d\n", record.id);
        printf("----------\n");
}

void print_host_info(hostinfo_t record, char* date) {
        printf("host: %s\n", record.host);
        printf("vhost: %d\n", record.vhost);
        printf("node: %d\n", record.node);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record.updatetime));
        printf("id: %d\n", record.id);
        printf("----------\n");
}

char is_empty(void * record, int bytes) {
    char * brec = (char*)record;
    while( bytes-- ) {
        if( *brec++ ) {
            return 0;
        }
    }
    return 1;
}

int main (int argc, char *argv[]) {
    FILE *pfile;
    nodeinfo_t node_record;
    balancerinfo_t balancer_record;
    contextinfo_t context_record;
    domaininfo_t domain_record;
    hostinfo_t host_record;
    char date[APR_RFC822_DATE_LEN];

    if (argc < 2) {
        printf("usage: %s filename\n", argv[0]);
        return 1;
    }

    pfile = fopen(argv[1], "rb");
    if (!pfile) {
        printf("Error: Cannot open the file %s\n", argv[1]);
        return 1;
    }

    if(strstr(argv[1], NODE_TABLE) != NULL) {
        fseek(pfile, OFFSET_NODE_TABLE, SEEK_SET);
        while (fread(&node_record, APR_ALIGN_DEFAULT( sizeof(nodeinfo_t) ), 1, pfile) == 1 ) {
            if (!is_empty(&node_record, APR_ALIGN_DEFAULT( sizeof(nodeinfo_t) ))) {
                print_node_info(node_record, date);
            }
        }
    } else if(strstr(argv[1], BALANCER_TABLE) != NULL) {
        fseek(pfile, OFFSET_BALANCER_TABLE, SEEK_SET);
        while (fread(&balancer_record, APR_ALIGN_DEFAULT( sizeof(balancerinfo_t) ), 1, pfile) == 1 ) {
            if (!is_empty(&balancer_record, APR_ALIGN_DEFAULT( sizeof(balancerinfo_t) ))) {
                print_balancer_info(balancer_record, date);
            }
        }
    } else if(strstr(argv[1], CONTEXT_TABLE) != NULL) {
        while (fread(&context_record, APR_ALIGN_DEFAULT( sizeof(contextinfo_t) ), 1, pfile) == 1 ) {
            fseek(pfile, OFFSET_CONTEXT_TABLE, SEEK_SET);
            if (!is_empty(&context_record, APR_ALIGN_DEFAULT( sizeof(contextinfo_t) ))) {
                print_context_info(context_record, date);
            }
        }
    } else if(strstr(argv[1], DOMAIN_TABLE) != NULL) {
        fseek(pfile, OFFSET_DOMAIN_TABLE, SEEK_SET);
        while (fread(&domain_record, APR_ALIGN_DEFAULT( sizeof(domaininfo_t) ), 1, pfile) == 1 ) {
            if (!is_empty(&domain_record, APR_ALIGN_DEFAULT( sizeof(domaininfo_t) ))) {
                print_domain_info(domain_record, date);
            }
        }
    } else if(strstr(argv[1], HOST_TABLE) != NULL) {
        fseek(pfile, OFFSET_HOST_TABLE, SEEK_SET);
        while (fread(&host_record, APR_ALIGN_DEFAULT( sizeof(hostinfo_t) ), 1, pfile) == 1 ) {
            if (!is_empty(&host_record, APR_ALIGN_DEFAULT( sizeof(hostinfo_t) ))) {
                print_host_info(host_record, date);
            }
        }
    } else {
        printf("Error: Expected one of %s, %s, %s, %s, %s in %s.\n",
            NODE_TABLE, BALANCER_TABLE, CONTEXT_TABLE, DOMAIN_TABLE, HOST_TABLE, argv[0]);
        return 1;
    }

    fclose(pfile);
    return 0;
}
