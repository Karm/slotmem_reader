/*
 * Toy mod_cluster structures reader
 *
 * Usage: ./reader  HTTPD_DIR/cache/mod_cluster/manager.node.nodes.slotmem
 *
 * @author Karm
 */

#include <stdio.h>
#include <assert.h>
#include "apr_file_io.h"
#include "httpd.h"
#include "apr.h"
#include "apr_general.h"
#include "apr_lib.h"
#include "slotmem.h"
#include "node.h"
#include "mod_proxy.h"
#include "host.h"
#include "apr_time.h"
#include "apr_strings.h"
#include "apr_shm.h"
#include "apr_pools.h"
#include "apr_file_info.h"
#include "unixd.h"


/* The description of the slots to reuse the slotmem */
struct sharedslotdesc {
    apr_size_t item_size;
    int item_num;
    unsigned int version; /* integer updated each time we make a change through the API */
};
struct ap_slotmem {
    char *name;
    apr_shm_t *shm;
    int *ident; /* integer table to process a fast alloc/free */
    unsigned int *version; /* address of version */
    void *base;
    apr_size_t size;
    int num;
    apr_pool_t *globalpool;
    apr_file_t *global_lock; /* file used for the locks */
    struct ap_slotmem *next;
};

/* global pool and list of slotmem we are handling */
static struct ap_slotmem *globallistmem = NULL;
static apr_pool_t *globalpool = NULL;
static apr_thread_mutex_t *globalmutex_lock = NULL;

/* Create the whole slotmem array */
static apr_status_t ap_slotmem_create(ap_slotmem_t **new, const char *name, apr_size_t item_size, int item_num, int persist, apr_pool_t *pool)
{
    char *ptr;
    struct sharedslotdesc desc, *new_desc;
    ap_slotmem_t *res;
    ap_slotmem_t *next = globallistmem;
    apr_status_t rv;
    const char *fname;
    const char *filename;
    apr_size_t nbytes;
    int i, *ident;
    apr_size_t dsize = APR_ALIGN_DEFAULT(sizeof(desc));
    apr_size_t tsize = APR_ALIGN_DEFAULT(sizeof(int) * (item_num + 1));

    item_size = APR_ALIGN_DEFAULT(item_size);
    nbytes = item_size * item_num + tsize + dsize;
    if (globalpool == NULL)
        return APR_ENOSHMAVAIL;
    if (name) {
        fname = name;

        /* first try to attach to existing slotmem */
        if (next) {
            for (;;) {
                if (strcmp(next->name, fname) == 0) {
                    /* we already have it */
                    *new = next;
                    return APR_SUCCESS;
                }
                if (!next->next) {
                    break;
                }
                next = next->next;
            }
        }
    }
    else {
        fname = "anonymous";
    }

    /* create the lock file and the global mutex */
    res = (ap_slotmem_t *) apr_pcalloc(globalpool, sizeof(ap_slotmem_t));
    filename = apr_pstrcat(pool, fname , ".lock", NULL);
    rv = apr_file_open(&res->global_lock, filename, APR_WRITE|APR_CREATE, APR_OS_DEFAULT, globalpool);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    if (globalmutex_lock == NULL)
        apr_thread_mutex_create(&globalmutex_lock, APR_THREAD_MUTEX_DEFAULT, globalpool);
    /* lock for creation */
    ap_slotmem_lock(res);

    /* first try to attach to existing shared memory */
    if (name) {
        rv = apr_shm_attach(&res->shm, fname, globalpool);
    }
    else {
        rv = APR_EINVAL;
    }
    if (rv == APR_SUCCESS) {
        /* check size */
        if (apr_shm_size_get(res->shm) != nbytes) {
            apr_shm_detach(res->shm);
            res->shm = NULL;
            ap_slotmem_unlock(res);
            return APR_EINVAL;
        }
        ptr = apr_shm_baseaddr_get(res->shm);
        memcpy(&desc, ptr, sizeof(desc));
        if (desc.item_size != item_size || desc.item_num != item_num) {
            apr_shm_detach(res->shm);
            res->shm = NULL;
            ap_slotmem_unlock(res);
            return APR_EINVAL;
        }
        new_desc = (struct sharedslotdesc *) ptr;
        ptr = ptr +  dsize;
    }
    else  {
        if (name) {
            int try = 0;
            rv = APR_EEXIST;
            while (rv == APR_EEXIST && try<5) {
                rv = apr_shm_remove(fname, globalpool);
                rv = apr_shm_create(&res->shm, nbytes, fname, globalpool);
                if (rv == APR_EEXIST) {
                     apr_sleep(apr_time_from_sec(1));
                }
                try++;
            }
        }
        else {
            rv = apr_shm_create(&res->shm, nbytes, NULL, globalpool);
        }
        if (rv != APR_SUCCESS) {
            ap_slotmem_unlock(res);
            return rv;
        }
        if (name) {
            /* Set permissions to shared memory
             * so it can be attached by child process
             * having different user credentials
             */
            unixd_set_shm_perms(fname);
        }
        ptr = apr_shm_baseaddr_get(res->shm);
        desc.item_size = item_size;
        desc.item_num = item_num;
        new_desc = (struct sharedslotdesc *) ptr;
        memcpy(ptr, &desc, sizeof(desc));
        ptr = ptr +  dsize;
        /* write the idents table */
        ident = (int *) ptr;
        for (i=0; i<item_num+1; i++) {
            ident[i] = i + 1;
        }
        /* clean the slots table */
        memset(ptr + sizeof(int) * (item_num + 1), 0, item_size * item_num);
        /* try to restore the _whole_ stuff from a persisted location */
        if (persist & CREPER_SLOTMEM)
            restore_slotmem(ptr, fname, item_size, item_num, pool);
    }

    /* For the chained slotmem stuff */
    res->name = apr_pstrdup(globalpool, fname);
    res->ident = (int *) ptr;
    res->base = ptr + tsize;
    res->size = item_size;
    res->num = item_num;
    res->version = &(new_desc->version);
    res->globalpool = globalpool;
    res->next = NULL;
    if (globallistmem==NULL) {
        globallistmem = res;
    }
    else {
        next->next = res;
    }

    *new = res;
    ap_slotmem_unlock(res);
    return APR_SUCCESS;
}

void restore_slotmem(void *ptr, apr_size_t item_size, int item_num, apr_pool_t *pool, const char *storename)
{
    apr_file_t *fp;
    apr_size_t nbytes;
    apr_status_t rv;

    item_size = APR_ALIGN_DEFAULT(item_size);
    nbytes = item_size * item_num + sizeof(int) * (item_num + 1);
    rv = apr_file_open(&fp, storename,  APR_READ | APR_WRITE, APR_OS_DEFAULT, pool);
    if (rv == APR_SUCCESS) {
        apr_finfo_t fi;
        if (apr_file_info_get(&fi, APR_FINFO_SIZE, fp) == APR_SUCCESS) {
            if (fi.size == nbytes) {
                apr_file_read(fp, ptr, &nbytes);
            }
            else {
                apr_file_close(fp);
                apr_file_remove(storename, pool);
                return;
            }
        }
        apr_file_close(fp);
    }
}


int main (int argc, char *argv[]) {
    //FILE *pfile;

    //hostinfo_t record;
    //nodeinfo_t record;


    //nodeinfo_t *ptr_one = (int *)malloc(sizeof(int));
    if (argc < 2) {
        printf("usage: %s filename\n", argv[0]);
        return 1;
    }
        apr_status_t rv;

    rv = apr_initialize();
    if (rv != APR_SUCCESS) {
        assert(0);
        return -1;
    }

    //pfile = fopen(argv[1],"rb");
    //if (!pfile) {
    //    printf("I cannot open the file %s", argv[1]);
    //    return 1;
    //}
//
    //apr_size_t nbytes;
    //apr_size_t item_size = sizeof(nodeinfo_t);
    //int item_num = 1;








    //item_size = APR_ALIGN_DEFAULT(item_size);
    //nbytes = item_size * item_num + sizeof(int) * (item_num + 1);
//
    //nodeinfo_t *record = (nodeinfo_t *)malloc(nbytes);
//
//
    //apr_size_t dsize = APR_ALIGN_DEFAULT(sizeof(record));
//
    //char *ptr;            restore_slargv[0]otmem(ptr, fname, item_size, item_num, pool);




/*
    //restore_slotmem(ptr, fname, item_size, item_num, pool);
    apr_file_t *fp;
    apr_pool_t *pool;
  
    apr_pool_create(&pool, NULL);


        apr_shm_t *shm;
    apr_size_t nbytes;

        char *ptr;
        ptr = apr_shm_baseaddr_get(res->shm);
        desc.item_size = item_size;
        desc.item_num = item_num;
        new_desc = (struct sharedslotdesc *) ptr;
        memcpy(ptr, &desc, sizeof(desc));
        ptr = ptr +  dsize;
        ident = (int *) ptr;
        for (i=0; i<item_num+1; i++) {
            ident[i] = i + 1;
        }
        memset(ptr + sizeof(int) * (item_num + 1), 0, item_size * item_num);
    restore_slotmem(ptr, item_size, item_num, pool, argv[0]);

*/



/*
    apr_shm_t *shm;

    apr_size_t nbytes;

    item_size = APR_ALIGN_DEFAULT(item_size);
    nbytes = item_size * item_num + sizeof(int) * (item_num + 1);

    rv = apr_shm_create(&shm, nbytes, NULL, pool);
        
        if (rv != APR_SUCCESS) {
            printf("Error :-) rv:%d",rv);
            return 1;
        }


    char *ptr;
    ptr = apr_shm_baseaddr_get(shm);


    rv = apr_file_open(&fp, argv[1], APR_READ | APR_WRITE, APR_OS_DEFAULT, pool);
*/

/*

if (rv == APR_SUCCESS) {
        apr_finfo_t fi;
        if (apr_file_info_get(&fi, APR_FINFO_SIZE, fp) == APR_SUCCESS) {
            if (fi.size == nbytes) {
                apr_file_read(fp, ptr, &nbytes);
            }
        }
        apr_file_close(fp);
    } else {
        printf("Error :-( rv:%d",rv);
            return 1;
        }


printf("nbytes:%lu",nbytes);
*/


//    while ( fread(&record, nbytes, 1, pfile) == 1 ) {
 //       printf("mess.balancer: %s\n", record.mess.balancer);
        //printf("mess.JVMRoute: %s\n", record.mess.JVMRoute);
        //printf("mess.Domain: %s\n", record.mess.Domain);
        //printf("mess.Host: %s\n", record.mess.Host);
        //printf("mess.Port: %s\n", record.mess.Port);
        //printf("mess.Type: %s\n", record.mess.Type);
        //printf("mess.reversed: %d\n", record.mess.reversed);
        //printf("mess.remove: %d\n", record.mess.remove);
        //printf("mess.flushpackets: %d\n", record.mess.flushpackets);
        //printf("mess.flushwait: %d\n", record.mess.flushwait);
        //printf("mess.ping: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.ping));
        //printf("mess.smax: %d\n", record.mess.smax);
        //printf("mess.ttl: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.ttl));
        //printf("mess.timeout: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.timeout));
        //printf("mess.id: %d\n", record.mess.id);
        //printf("mess.updatetimelb: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.updatetimelb));
        //printf("mess.num_failure_idle: %d\n", record.mess.num_failure_idle);
        //printf("mess.oldelected: %d\n", record.mess.oldelected);
        //printf("mess.lastcleantry: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.mess.lastcleantry));
        //printf("updatetime: %" APR_TIME_T_FMT " [ms]\n", apr_time_as_msec(record.updatetime));
        //printf("offset: %d\n", record.offset);
        //printf("stat: %s\n", record.stat);
        //printf("----------\n");
   // }

    //fclose(pfile);
    return 0;
}
