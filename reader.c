/*
 * Toy mod_cluster structures reader
 *
 * Usage:
 *       ./reader HTTPD_DIR/cache/mod_cluster/manager.balancer.balancers.slotmem
 *       ./reader HTTPD_DIR/cache/mod_cluster/manager.context.contexts.slotmem
 *       ./reader HTTPD_DIR/cache/mod_cluster/manager.domain.domain.slotmem
 *       ./reader HTTPD_DIR/cache/mod_cluster/manager.host.hosts.slotmem
 *       ./reader HTTPD_DIR/cache/mod_cluster/manager.node.nodes.slotmem
 *
 * @std C99
 * @author Michal Karm Babacek
 */

#include <stdio.h>
#include "apr_time.h"
#include "apr_file_io.h"
#include "apr_pools.h"
#include "apr_file_info.h"
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

#define POSSIBLE_BLOCKS 2
#define CMP_BLOCK_SIZE  7
const unsigned char possible_ends_of_alignmnet_blocks[POSSIBLE_BLOCKS][CMP_BLOCK_SIZE] = {
    { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

/* originally defined in sharedmem_util.c */
struct ap_slotmem {
    char *name;
    apr_shm_t *shm;
    int *ident;
    unsigned int *version;
    void *base;
    apr_size_t size;
    int num;
    apr_pool_t *globalpool;
    apr_file_t *global_lock;
    struct ap_slotmem *next;
};

void print_node_info(nodeinfo_t* record, char* date) {
        printf("mess.balancer: %s\n", record->mess.balancer);
        printf("mess.JVMRoute: %s\n", record->mess.JVMRoute);
        printf("mess.Domain: %s\n", record->mess.Domain);
        printf("mess.Host: %s\n", record->mess.Host);
        printf("mess.Port: %s\n", record->mess.Port);
        printf("mess.Type: %s\n", record->mess.Type);
        printf("mess.reversed: %d\n", record->mess.reversed);
        printf("mess.remove: %d\n", record->mess.remove);
        printf("mess.flushpackets: %d\n", record->mess.flushpackets);
        printf("mess.flushwait: %d\n", record->mess.flushwait);
        printf("mess.ping: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record->mess.ping));
        printf("mess.smax: %d\n", record->mess.smax);
        printf("mess.ttl: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record->mess.ttl));
        printf("mess.timeout: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record->mess.timeout));
        printf("mess.id: %d\n", record->mess.id);
        apr_rfc822_date(date,record->mess.updatetimelb);
        printf("mess.updatetimelb: %s\n",date);
        printf("mess.num_failure_idle: %d\n", record->mess.num_failure_idle);
        printf("mess.oldelected: %d\n", record->mess.oldelected);
        printf("mess.lastcleantry: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record->mess.lastcleantry));
        apr_rfc822_date(date,record->updatetime);
        printf("updatetime: %s\n",date);
        printf("offset: %d\n", record->offset);
        printf("stat: %s\n", record->stat);
        printf("----------\n");
}

void print_balancer_info(balancerinfo_t* record, char* date) {
        printf("balancer: %s\n", record->balancer);
        printf("StickySession: %d\n", record->StickySession);
        printf("StickySessionCookie: %s\n", record->StickySessionCookie);
        printf("StickySessionPath: %s\n", record->StickySessionPath);
        printf("StickySessionRemove: %d\n", record->StickySessionRemove);
        printf("StickySessionForce: %d\n", record->StickySessionForce);
        printf("Timeout: %d\n", record->Timeout);
        printf("Maxattempts: %d\n", record->Maxattempts);
        //apr_rfc822_date(date,record->updatetime);
        //printf("updatetime: %s\n",date);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record->updatetime));
        printf("id: %d\n", record->id);
        printf("----------\n");
}

void print_context_info(contextinfo_t* record, char* date) {
        printf("context: %s\n", record->context);
        printf("vhost: %d\n", record->vhost);
        printf("node: %d\n", record->node);
        printf("status: %d\n", record->status);
        printf("nbrequests: %d\n", record->nbrequests);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record->updatetime));
        printf("id: %d\n", record->id);
        printf("----------\n");
}

void print_domain_info(domaininfo_t* record, char* date) {
        printf("domain: %s\n", record->domain);
        printf("JVMRoute: %s\n", record->JVMRoute);
        printf("balancer: %s\n", record->balancer);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record->updatetime));
        printf("id: %d\n", record->id);
        printf("----------\n");
}

void print_host_info(hostinfo_t* record, char* date) {
        printf("host: %s\n", record->host);
        printf("vhost: %d\n", record->vhost);
        printf("node: %d\n", record->node);
        printf("updatetime: %" APR_TIME_T_FMT " [s]\n", apr_time_sec(record->updatetime));
        printf("id: %d\n", record->id);
        printf("----------\n");
}

apr_status_t is_empty(void * record, int bytes) {
    char * brec = (char*)record;
    while( bytes-- ) {
        if( *brec++ ) {
            return FALSE;
        }
    }
    return TRUE;
}

/**
  * Memory dumped into the file contains aligned space between slotmem structure and
  * the chain of actual structures we read. This function guesses the end of such block.
  * It allows us to read binary files from different platforms.
  */
apr_status_t is_end_of_alignment_block(apr_file_t* fp, char *a){
    a++; // Skip the first, it's the last of the previous struc.
    char peep_at_one;
    apr_size_t peep_at_one_s = sizeof(char);
    apr_off_t offset = - sizeof(char);
    for(int i = 0; i < POSSIBLE_BLOCKS; i++) {
        if(memcmp(a, possible_ends_of_alignmnet_blocks[i], sizeof(possible_ends_of_alignmnet_blocks[i])) == 0) {
            if (apr_file_read(fp, &peep_at_one, &peep_at_one_s) != APR_SUCCESS) {
                // File full of zeroes...
                return FALSE;
            }
            apr_file_seek(fp, APR_CUR, &offset);
            if (peep_at_one != 0x00) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

apr_status_t eat_all_alignment_blocks(apr_file_t* fp){
    apr_size_t s_alignment = (CMP_BLOCK_SIZE+1) * sizeof(char);
    char alignment[CMP_BLOCK_SIZE+1];
    for (;;) {
        if (apr_file_read(fp, &alignment, &s_alignment) != APR_SUCCESS) {
            // File full of zeroes...
            return APR_EGENERAL;
        }
        if(is_end_of_alignment_block(fp, alignment)) {
            break;
        }
    }
    return APR_SUCCESS;
}

apr_status_t eat_slotmem_struct(apr_file_t* fp, apr_pool_t* pool) {
    ap_slotmem_t *slotmem;
    slotmem = apr_pcalloc(pool, APR_ALIGN_DEFAULT( sizeof(ap_slotmem_t) ));
    if (!slotmem) {
        printf("APR apr_pcalloc err\n");
        return APR_EGENERAL;
    }
    apr_size_t nbytes = APR_ALIGN_DEFAULT( sizeof(ap_slotmem_t) );
    apr_file_read(fp, slotmem, &nbytes);
    // Silence is golden, but we may spit some info about the structure
    return APR_SUCCESS;
}

apr_status_t process_node(apr_pool_t* pool, apr_file_t* fp, char* date) {
    void *record;
    record = (nodeinfo_t*)apr_pcalloc(pool, APR_ALIGN_DEFAULT( sizeof(nodeinfo_t) ));
    if (!record) {
        printf("APR apr_pcalloc err\n");
        return APR_EGENERAL;
    }
    // Actual written size of the structure in mem
    apr_size_t nbytes = APR_ALIGN_DEFAULT( sizeof(nodeinfo_t) )/2-40;
    for (;;) {
        if (apr_file_read(fp, record, &nbytes) != APR_SUCCESS || is_empty(record, nbytes)) {
            break;
        }
        print_node_info(record, date);
    }
    return APR_SUCCESS;
}

apr_status_t process_balancer(apr_pool_t* pool, apr_file_t* fp, char* date) {
    void *record;
    record = (balancerinfo_t*) apr_pcalloc(pool, APR_ALIGN_DEFAULT( sizeof(balancerinfo_t) ));
    if (!record) {
        printf("APR apr_pcalloc err\n");
        return APR_EGENERAL;
    }
    apr_size_t nbytes = APR_ALIGN_DEFAULT( sizeof(balancerinfo_t) );
    for (;;) {
        if (apr_file_read(fp, record, &nbytes) != APR_SUCCESS || is_empty(record, nbytes)) {
            break;
        }
        print_balancer_info(record, date);
    }
    return APR_SUCCESS;
}

apr_status_t process_context(apr_pool_t* pool, apr_file_t* fp, char* date) {
    void *record;
    record = (contextinfo_t*) apr_pcalloc(pool, APR_ALIGN_DEFAULT( sizeof(contextinfo_t) ));
    if (!record) {
        printf("APR apr_pcalloc err\n");
        return APR_EGENERAL;
    }
    apr_size_t nbytes = APR_ALIGN_DEFAULT( sizeof(contextinfo_t) );
    for (;;) {
        if (apr_file_read(fp, record, &nbytes) != APR_SUCCESS || is_empty(record, nbytes)) {
            break;
        }
        print_context_info(record, date);
    }
    return APR_SUCCESS;
}

apr_status_t process_domain(apr_pool_t* pool, apr_file_t* fp, char* date) {
    void *record;
    record = (domaininfo_t*) apr_pcalloc(pool, APR_ALIGN_DEFAULT( sizeof(domaininfo_t) ));
    if (!record) {
        printf("APR apr_pcalloc err\n");
        return APR_EGENERAL;
    }
    apr_size_t nbytes = APR_ALIGN_DEFAULT( sizeof(domaininfo_t) );
    for (;;) {
        if (apr_file_read(fp, record, &nbytes) != APR_SUCCESS || is_empty(record, nbytes)) {
            break;
        }
        print_domain_info(record, date);
    }
    return APR_SUCCESS;
}

apr_status_t process_host(apr_pool_t* pool, apr_file_t* fp, char* date) {
    void *record;
    record = (hostinfo_t*) apr_pcalloc(pool, APR_ALIGN_DEFAULT( sizeof(hostinfo_t) ));
    if (!record) {
        printf("APR apr_pcalloc err\n");
        return APR_EGENERAL;
    }
    apr_size_t nbytes = APR_ALIGN_DEFAULT( sizeof(hostinfo_t) );
    for (;;) {
        if (apr_file_read(fp, record, &nbytes) != APR_SUCCESS || is_empty(record, nbytes)) {
            break;
        }
        print_host_info(record, date);
    }
    return APR_SUCCESS;
}

int main (int argc, char *argv[]) {
    apr_status_t rv;
    apr_pool_t *pool;
    apr_pool_t *pool_data;
    apr_file_t *fp;
    char date[APR_RFC822_DATE_LEN];

    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        return APR_EGENERAL;
    }

    rv = apr_initialize();
    if (rv != APR_SUCCESS) {
        printf("APR apr_initialize err\n");
        return APR_EGENERAL;
    }

    apr_pool_create(&pool_data, NULL);
    apr_pool_create(&pool, NULL);

    rv = apr_file_open(&fp, argv[1], APR_READ | APR_FOPEN_BINARY, APR_OS_DEFAULT, pool);
    if (rv != APR_SUCCESS) {
        printf("Error: Cannot open the file %s\n", argv[1]);
        return APR_EGENERAL;
    }

    apr_finfo_t fi;
    if (apr_file_info_get(&fi, APR_FINFO_SIZE, fp) == APR_SUCCESS) {
        if (fi.size < CMP_BLOCK_SIZE+2) {
            apr_file_close(fp);
            printf("Error: The file %s is too short. It most likely does not contain anything useful.\n", argv[1]);
            return APR_EGENERAL;
        }
    }

    if(strstr(argv[1], NODE_TABLE) != NULL) {
        eat_slotmem_struct(fp, pool);
        if(eat_all_alignment_blocks(fp) == APR_SUCCESS) {
            process_node(pool, fp, date);
        }
    } else if(strstr(argv[1], BALANCER_TABLE) != NULL) {
        eat_slotmem_struct(fp, pool);
        if(eat_all_alignment_blocks(fp) == APR_SUCCESS) {
            process_balancer(pool, fp, date);
        }
    } else if(strstr(argv[1], CONTEXT_TABLE) != NULL) {
        eat_slotmem_struct(fp, pool);
        if(eat_all_alignment_blocks(fp) == APR_SUCCESS) {
            process_context(pool, fp, date);
        }
    } else if(strstr(argv[1], DOMAIN_TABLE) != NULL) {
        eat_slotmem_struct(fp, pool);
        if(eat_all_alignment_blocks(fp) == APR_SUCCESS) {
            process_domain(pool, fp, date);
        }
    } else if(strstr(argv[1], HOST_TABLE) != NULL) {
        eat_slotmem_struct(fp, pool);
        if(eat_all_alignment_blocks(fp) == APR_SUCCESS) {
            process_host(pool, fp, date);
        }
    } else {
        printf("Error: Expected one of %s, %s, %s, %s, %s in %s.\n",
            NODE_TABLE, BALANCER_TABLE, CONTEXT_TABLE, DOMAIN_TABLE, HOST_TABLE, argv[0]);
        return 1;
    }

    printf("\nDONE displaying available data.\n");

    apr_file_close(fp);
    apr_pool_destroy(pool);
    apr_pool_destroy(pool_data);
    apr_terminate();

    return 0;
}
