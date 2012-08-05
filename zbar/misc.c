/*------------------------------------------------------------------------
 *  Copyright 2012 (c) Jarek Czekalski <jarekczek@poczta.onet.pl>
 *
 *  This file is part of the ZBar Bar Code Reader.
 *
 *  The ZBar Bar Code Reader is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU Lesser Public License as
 *  published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  The ZBar Bar Code Reader is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser Public License
 *  along with the ZBar Bar Code Reader; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *  Boston, MA  02110-1301  USA
 *
 *  http://sourceforge.net/projects/zbar
 *------------------------------------------------------------------------*/

#include "misc.h"
#include "error.h"

static int module_initialized = 0;
static errinfo_t err;

static void module_init()
{
    if (module_initialized)
        return;
    err_init(&err, ZBAR_MOD_UNKNOWN);
    module_initialized = 1;
}

void resol_list_init(resol_list_t *list)
{
    module_init();
    list->cnt = 0;
    // an empty list consists of 1 zeroed element
    list->resols = calloc(1, sizeof(resol_t));
    if (!list->resols)
    {
        err_capture(&err, SEV_FATAL, ZBAR_ERR_NOMEM,
            __func__, "allocating resources");
    }
}

void resol_list_cleanup(resol_list_t *list)
{
    free(list->resols);
}

void resol_list_add(resol_list_t *list, resol_t *resol)
{
    list->cnt++;
    list->resols = realloc(list->resols, (list->cnt + 1) * sizeof(resol_t));
    if (!list->resols)
    {
        err_capture(&err, SEV_FATAL, ZBAR_ERR_NOMEM,
            __func__, "allocating resources");
    }
    list->resols[list->cnt - 1] = *resol;
    memset(&list->resols[list->cnt], 0, sizeof(resol_t));
}

void get_closest_resol(resol_t *resol, resol_list_t *list)
{
    resol_t *test_res;
    long min_diff = 0;
    long ind_best = -1; // the index of best resolution in resols
    int i = 0;
    for (test_res = list->resols; !struct_null(test_res); test_res++)
    {
        long diff;
        if (resol->cx)
        {
            diff = test_res->cx - resol->cx;
            if (diff < 0)
                diff = -diff;
        }
        else
        {
            // empty resol, looking for the biggest
            diff = -test_res->cx;
        }
        if (ind_best < 0 || diff < min_diff)
        {
            ind_best = i;
            min_diff = diff;
        }
        i++;
    }
    
    if (ind_best >= 0)
    {
        *resol = list->resols[ind_best];
    }
}

int struct_null_fun(const void *pdata, const int len)
    {
        int i;
        for (i = 0; i < len; i++)
        {
            if (((char*)pdata)[i] != 0)
            {
                return 0;
            }
        }
        return 1;
    }

// :tabSize=4:indentSize=4:noTabs=true:
