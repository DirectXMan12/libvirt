/*
 * virsh-completer.c: Common custom completion utilities
 *
 * Copyright (C) 2005, 2007-2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "virsh-completer.h"

#include <stdarg.h>

#include "conf/domain_conf.h"
#include "viralloc.h"

/* Utils - General */
char **
vshVarArgsToStringList(vshControl *ctl, unsigned int count, ...)
{
    va_list ap;
    char **strs;
    size_t i;

    if (count == 0)
        return NULL;

    va_start(ap, count);

    strs = vshCalloc(ctl, count, sizeof(char*));
    for (i = 0; i < count; i++)
        strs[i] = vshStrdup(ctl, va_arg(ap, char*));

    va_end(ap);

    return strs;
};

/* Utils - Domain Listing */
/* compare domains, pack NULLed ones at the end*/
static int
vshDomainSorter(const void *a, const void *b)
{
    virDomainPtr *da = (virDomainPtr *) a;
    virDomainPtr *db = (virDomainPtr *) b;
    unsigned int ida;
    unsigned int idb;
    unsigned int inactive = (unsigned int) -1;

    if (*da && !*db)
        return -1;

    if (!*da)
        return *db != NULL;

    ida = virDomainGetID(*da);
    idb = virDomainGetID(*db);

    if (ida == inactive && idb == inactive)
        return vshStrcasecmp(virDomainGetName(*da), virDomainGetName(*db));

    if (ida != inactive && idb != inactive) {
        if (ida > idb)
            return 1;
        else if (ida < idb)
            return -1;
    }

    if (ida != inactive)
        return -1;
    else
        return 1;
}

void
vshDomainListFree(vshDomainListPtr domlist)
{
    size_t i;

    if (domlist && domlist->domains) {
        for (i = 0; i < domlist->ndomains; i++) {
            if (domlist->domains[i])
                virDomainFree(domlist->domains[i]);
        }
        VIR_FREE(domlist->domains);
    }
    VIR_FREE(domlist);
}

vshDomainListPtr
vshDomainListCollect(vshControl *ctl, unsigned int flags)
{
    vshDomainListPtr list = vshMalloc(ctl, sizeof(*list));
    size_t i;
    int ret;
    int *ids = NULL;
    int nids = 0;
    char **names = NULL;
    int nnames = 0;
    virDomainPtr dom;
    bool success = false;
    size_t deleted = 0;
    int persistent;
    int autostart;
    int state;
    int nsnap;
    int mansave;

    /* try the list with flags support (0.9.13 and later) */
    if ((ret = virConnectListAllDomains(ctl->conn, &list->domains,
                                        flags)) >= 0) {
        list->ndomains = ret;
        goto finished;
    }

    /* check if the command is actually supported */
    if (last_error && last_error->code == VIR_ERR_NO_SUPPORT) {
        vshResetLibvirtError();
        goto fallback;
    }

    if (last_error && last_error->code ==  VIR_ERR_INVALID_ARG) {
        /* try the new API again but mask non-guaranteed flags */
        unsigned int newflags = flags & (VIR_CONNECT_LIST_DOMAINS_ACTIVE |
                                         VIR_CONNECT_LIST_DOMAINS_INACTIVE);

        vshResetLibvirtError();
        if ((ret = virConnectListAllDomains(ctl->conn, &list->domains,
                                            newflags)) >= 0) {
            list->ndomains = ret;
            goto filter;
        }
    }

    /* there was an error during the first or second call */
    vshError(ctl, "%s", _("Failed to list domains"));
    goto cleanup;


 fallback:
    /* fall back to old method (0.9.12 and older) */
    vshResetLibvirtError();

    /* list active domains, if necessary */
    if (!VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_FILTERS_ACTIVE) ||
        VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_ACTIVE)) {
        if ((nids = virConnectNumOfDomains(ctl->conn)) < 0) {
            vshError(ctl, "%s", _("Failed to list active domains"));
            goto cleanup;
        }

        if (nids) {
            ids = vshMalloc(ctl, sizeof(int) * nids);

            if ((nids = virConnectListDomains(ctl->conn, ids, nids)) < 0) {
                vshError(ctl, "%s", _("Failed to list active domains"));
                goto cleanup;
            }
        }
    }

    if (!VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_FILTERS_ACTIVE) ||
        VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_INACTIVE)) {
        if ((nnames = virConnectNumOfDefinedDomains(ctl->conn)) < 0) {
            vshError(ctl, "%s", _("Failed to list inactive domains"));
            goto cleanup;
        }

        if (nnames) {
            names = vshMalloc(ctl, sizeof(char *) * nnames);

            if ((nnames = virConnectListDefinedDomains(ctl->conn, names,
                                                      nnames)) < 0) {
                vshError(ctl, "%s", _("Failed to list inactive domains"));
                goto cleanup;
            }
        }
    }

    list->domains = vshMalloc(ctl, sizeof(virDomainPtr) * (nids + nnames));
    list->ndomains = 0;

    /* get active domains */
    for (i = 0; i < nids; i++) {
        if (!(dom = virDomainLookupByID(ctl->conn, ids[i])))
            continue;
        list->domains[list->ndomains++] = dom;
    }

    /* get inactive domains */
    for (i = 0; i < nnames; i++) {
        if (!(dom = virDomainLookupByName(ctl->conn, names[i])))
            continue;
        list->domains[list->ndomains++] = dom;
    }

    /* truncate domains that weren't found */
    deleted = (nids + nnames) - list->ndomains;

 filter:
    /* filter list the list if the list was acquired by fallback means */
    for (i = 0; i < list->ndomains; i++) {
        dom = list->domains[i];

        /* persistence filter */
        if (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_FILTERS_PERSISTENT)) {
            if ((persistent = virDomainIsPersistent(dom)) < 0) {
                vshError(ctl, "%s", _("Failed to get domain persistence info"));
                goto cleanup;
            }

            if (!((VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_PERSISTENT) && persistent) ||
                  (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_TRANSIENT) && !persistent)))
                goto remove_entry;
        }

        /* domain state filter */
        if (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_FILTERS_STATE)) {
            if (virDomainGetState(dom, &state, NULL, 0) < 0) {
                vshError(ctl, "%s", _("Failed to get domain state"));
                goto cleanup;
            }

            if (!((VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_RUNNING) &&
                   state == VIR_DOMAIN_RUNNING) ||
                  (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_PAUSED) &&
                   state == VIR_DOMAIN_PAUSED) ||
                  (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_SHUTOFF) &&
                   state == VIR_DOMAIN_SHUTOFF) ||
                  (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_OTHER) &&
                   (state != VIR_DOMAIN_RUNNING &&
                    state != VIR_DOMAIN_PAUSED &&
                    state != VIR_DOMAIN_SHUTOFF))))
                goto remove_entry;
        }

        /* autostart filter */
        if (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_FILTERS_AUTOSTART)) {
            if (virDomainGetAutostart(dom, &autostart) < 0) {
                vshError(ctl, "%s", _("Failed to get domain autostart state"));
                goto cleanup;
            }

            if (!((VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_AUTOSTART) && autostart) ||
                  (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_NO_AUTOSTART) && !autostart)))
                goto remove_entry;
        }

        /* managed save filter */
        if (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_FILTERS_MANAGEDSAVE)) {
            if ((mansave = virDomainHasManagedSaveImage(dom, 0)) < 0) {
                vshError(ctl, "%s",
                         _("Failed to check for managed save image"));
                goto cleanup;
            }

            if (!((VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_MANAGEDSAVE) && mansave) ||
                  (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_NO_MANAGEDSAVE) && !mansave)))
                goto remove_entry;
        }

        /* snapshot filter */
        if (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_FILTERS_SNAPSHOT)) {
            if ((nsnap = virDomainSnapshotNum(dom, 0)) < 0) {
                vshError(ctl, "%s", _("Failed to get snapshot count"));
                goto cleanup;
            }
            if (!((VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_HAS_SNAPSHOT) && nsnap > 0) ||
                  (VSH_MATCH(VIR_CONNECT_LIST_DOMAINS_NO_SNAPSHOT) && nsnap == 0)))
                goto remove_entry;
        }

        /* the domain matched all filters, it may stay */
        continue;

 remove_entry:
        /* the domain has to be removed as it failed one of the filters */
        virDomainFree(list->domains[i]);
        list->domains[i] = NULL;
        deleted++;
    }

 finished:
    /* sort the list */
    if (list->domains && list->ndomains)
        qsort(list->domains, list->ndomains, sizeof(*list->domains),
              vshDomainSorter);

    /* truncate the list if filter simulation deleted entries */
    if (deleted)
        VIR_SHRINK_N(list->domains, list->ndomains, deleted);

    success = true;

 cleanup:
    for (i = 0; nnames != -1 && i < nnames; i++)
        VIR_FREE(names[i]);

    if (!success) {
        vshDomainListFree(list);
        list = NULL;
    }

    VIR_FREE(names);
    VIR_FREE(ids);
    return list;
}


/* Common completers */
char **
vshCompleteDomain(unsigned int flags)
{
    vshControl *ctl = vshGetCompleterCtl();
    vshDomainListPtr dom_list = NULL;
    virDomainPtr *domains = NULL;
    char **names = NULL;
    size_t i;

    if (!ctl)
        return NULL;

    if (flags == 0)
        flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE |
                VIR_CONNECT_LIST_DOMAINS_INACTIVE;

    dom_list = vshDomainListCollect(ctl, flags);

    if (!dom_list)
        return NULL;

    domains = dom_list->domains;
    names = vshCalloc(ctl, dom_list->ndomains+1, sizeof(char*));

    for (i = 0; i < dom_list->ndomains; i++)
        names[i] = vshStrdup(ctl, virDomainGetName(domains[i]));

    names[dom_list->ndomains] = NULL;

    vshDomainListFree(dom_list);

    return names;
};
