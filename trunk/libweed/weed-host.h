/* WEED is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   Weed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


   Weed is developed by:

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   mainly based on LiViDO, which is developed by:


   Niels Elburg - http://veejay.sf.net

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   Denis "Jaromil" Rojo - http://freej.dyne.org

   Tom Schouten - http://zwizwa.fartit.com

   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:

   Silvano "Kysucix" Galliani - http://freej.dyne.org

   Kentaro Fukuchi - http://megaui.net/fukuchi

   Jun Iio - http://www.malib.net

   Carlo Prelz - http://www2.fluido.as:8080/

*/

/* (C) Gabriel "Salsaman" Finch, 2005 - 2007 */

#ifndef __WEED_HOST_H__
#define __WEED_HOST_H__

#ifndef __WEED_H__
#include <weed/weed.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Plant types */
#define WEED_PLANT_UNKNOWN 0 ///< used for host deserialisation only

/* Caller Types */
#define WEED_CALLER_HOST 0
#define WEED_CALLER_PLUGIN 1

  /* host only functions */
typedef void (*weed_plant_free_f)(weed_plant_t *plant);
typedef int (*weed_leaf_delete_f)(weed_plant_t *plant, const char *key);
typedef int (*weed_leaf_set_flags_f)(weed_plant_t *plant, const char *key, int flags);
typedef int (weed_leaf_set_caller_f)(weed_plant_t *plant, const char *key, int seed_type, int num_elems, void *value, int caller);

#ifndef _SKIP_WEED_API_
weed_default_getter_f weed_default_get;
weed_leaf_get_f weed_leaf_get;
weed_leaf_set_f weed_leaf_set;
weed_leaf_set_f weed_leaf_set_plugin;
weed_plant_new_f weed_plant_new;
weed_plant_free_f weed_plant_free;
weed_leaf_delete_f weed_leaf_delete;
weed_plant_list_leaves_f weed_plant_list_leaves;
weed_leaf_num_elements_f weed_leaf_num_elements;
weed_leaf_element_size_f weed_leaf_element_size;
weed_leaf_seed_type_f weed_leaf_seed_type;
weed_leaf_get_flags_f weed_leaf_get_flags;
weed_leaf_set_flags_f weed_leaf_set_flags;

weed_malloc_f weed_malloc;
weed_free_f weed_free;
weed_memcpy_f weed_memcpy;
weed_memset_f weed_memset;

#endif

void weed_init(int api_v, weed_malloc_f, weed_free_f, weed_memcpy_f, weed_memset_f);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_HOST_H__


