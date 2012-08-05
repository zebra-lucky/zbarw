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
#ifndef _MISC_H_
#define _MISC_H_

struct resol_s {
    long cx;
    long cy;
};
typedef struct resol_s resol_t;

struct resol_list_s {
    resol_t *resols;
    long cnt;
};
typedef struct resol_list_s resol_list_t;

void resol_list_init(resol_list_t *list);
void resol_list_cleanup(resol_list_t *list);
void resol_list_add(resol_list_t *list, resol_t *resol);
/// Fill <code>resol</code> with the closest resolution found in
/// <code>list</code>.
/** If <code>list</code> is empty,
  * the <code>resol</code> is unchanged. If <code>resol</code> is empty,
  * the biggest resolution is chosen. */
void get_closest_resol(resol_t *resol, resol_list_t *list);

/// Returns 1 if the struct is null, otherwise 0
int struct_null_fun(const void *pdata, const int len);
/// Returns 1 if the struct is null, otherwise 0
#define struct_null(pdata) struct_null_fun(pdata, sizeof(*pdata))

#endif
