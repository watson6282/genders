/*****************************************************************************\
 *  $Id: genders.c,v 1.133 2005-05-07 15:30:42 achu Exp $
 *****************************************************************************
 *  Copyright (C) 2001-2003 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov> and Albert Chu <chu11@llnl.gov>.
 *  UCRL-CODE-2003-004.
 *
 *  This file is part of Genders, a cluster configuration database.
 *  For details, see <http://www.llnl.gov/linux/genders/>.
 *
 *  Genders is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Genders is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Genders; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <errno.h>
#include <assert.h>

#include "genders.h"
#include "genders_api.h"
#include "genders_constants.h"
#include "genders_parsing.h"
#include "genders_util.h"
#include "hash.h"
#include "list.h"

/* 
 * genders_errmsg
 *
 * store errormsg strings
 */
static char * genders_errmsg[] = 
  {
    "success",
    "genders handle is null",
    "error opening genders file",
    "error reading genders file",
    "genders file parse error",
    "genders data not loaded",
    "genders data already loaded",
    "array or string passed in not large enough to store result",
    "incorrect parameters passed in",
    "null pointer reached in list", 
    "node or attribute not found",
    "out of memory",
    "query syntax error",
    "genders handle magic number incorrect, improper handle passed in",
    "unknown internal error",
    "error number out of range",
  };

/* 
 * _initialize_handle_info
 *
 * Initialize genders_t handle
 */
static void 
_initialize_handle_info(genders_t handle)
{
  assert(handle);

  handle->magic = GENDERS_MAGIC_NUM;
  handle->is_loaded = 0;
  handle->numnodes = 0;
  handle->numattrs = 0;
  handle->maxattrs = 0;
  handle->maxnodelen = 0;
  handle->maxattrlen = 0;
  handle->maxvallen = 0;
  memset(handle->nodename,'\0',GENDERS_MAXHOSTNAMELEN+1);
  handle->valbuf = NULL;
  handle->node_index = NULL;
  handle->attr_index = NULL;
  handle->attrval_index = NULL;
  handle->attrval_index_attr = NULL;

  /* Don't initialize the nodeslist, attrvalslist, or attrslist, they
   * should not be re-initialized on a load_data error.
   */
}

genders_t 
genders_handle_create(void) 
{
  genders_t handle = NULL;

  /* Don't use the wrapper here, no errnum to set */
  if (!(handle = (genders_t)malloc(sizeof(struct genders))))
    goto cleanup;
  
  _initialize_handle_info(handle);
  handle->nodeslist = NULL;
  handle->attrvalslist = NULL;
  handle->attrslist = NULL;
  handle->attrval_buflist = NULL;
  
  __list_create(handle->nodeslist, _free_genders_node);
  __list_create(handle->attrvalslist, _free_attrvallist);
  __list_create(handle->attrslist, free);
  
  /* node_index and attr_index created in genders_load_data after node
   * and attr counts are determined.  Valbuf created in
   * genders_load_data after maxvallen is calculated
   */
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle;
  
 cleanup:
  if (handle) 
    {
      __list_destroy(handle->nodeslist);
      __list_destroy(handle->attrvalslist);
      __list_destroy(handle->attrslist);
      free(handle);
    }
  return NULL;
}

int 
genders_handle_destroy(genders_t handle)
{
  if (_handle_error_check(handle) < 0)
    return -1;

  __list_destroy(handle->nodeslist);
  __list_destroy(handle->attrvalslist);
  __list_destroy(handle->attrslist);
  free(handle->valbuf);
  __hash_destroy(handle->node_index);
  __hash_destroy(handle->attr_index);
  __hash_destroy(handle->attrval_index);
  free(handle->attrval_index_attr);
  __list_destroy(handle->attrval_buflist);

  /* "clean" handle */
  _initialize_handle_info(handle);
  handle->magic = ~GENDERS_MAGIC_NUM; /* ~0xdeadbeef == 0xlivebeef :-) */
  handle->is_loaded = 0;
  handle->nodeslist = NULL;
  handle->attrvalslist = NULL;
  handle->attrslist = NULL;
  free(handle);
  return 0;
}

int
genders_load_data(genders_t handle, const char *filename) 
{
  char *temp;

  if (_unloaded_handle_error_check(handle) < 0)
    goto cleanup;

  if (genders_open_and_parse(handle, 
			     filename, 
			     handle->nodeslist, 
			     handle->attrvalslist,
			     0, 
			     NULL) < 0)
    goto cleanup;

  handle->numnodes = list_count(handle->nodeslist);

  if (gethostname(handle->nodename, GENDERS_MAXHOSTNAMELEN+1) < 0) 
    {
      handle->errnum = GENDERS_ERR_INTERNAL;
      goto cleanup;
    }
  handle->nodename[GENDERS_MAXHOSTNAMELEN]='\0';

  /* shorten hostname if necessary */
  if ((temp = strchr(handle->nodename,'.')))
    *temp = '\0';
  
  handle->maxnodelen = GENDERS_MAX(strlen(handle->nodename), handle->maxnodelen);

  /* Create a buffer for value substitutions */
  __xmalloc(handle->valbuf, char *, handle->maxvallen + 1);

  if (genders_index_nodes(handle) < 0)
    goto cleanup;

  if (genders_index_attrs(handle) < 0)
    goto cleanup;

  handle->is_loaded++;
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;

cleanup:
  if (handle && handle->magic == GENDERS_ERR_MAGIC) 
    {
      free(handle->valbuf);
      
      /* Can't pass NULL for key, so pass junk, _is_all() will ensure
       * everything is deleted
       */
      list_delete_all(handle->nodeslist, _is_all, ""); 
      list_delete_all(handle->attrvalslist, _is_all, ""); 
      list_delete_all(handle->attrslist, _is_all, ""); 
      __hash_destroy(handle->node_index);
      __hash_destroy(handle->attr_index);
      _initialize_handle_info(handle);
    }
  return -1;
}

int 
genders_errnum(genders_t handle) 
{
  if (!handle)
    return GENDERS_ERR_NULLHANDLE;
  else if (handle->magic != GENDERS_MAGIC_NUM)
    return GENDERS_ERR_MAGIC;
  else
    return handle->errnum;
}

char *
genders_strerror(int errnum) 
{
  if (errnum >= GENDERS_ERR_SUCCESS && errnum <= GENDERS_ERR_ERRNUMRANGE)
    return genders_errmsg[errnum];
  else
    return genders_errmsg[GENDERS_ERR_ERRNUMRANGE];
}

char *
genders_errormsg(genders_t handle) 
{
  return genders_strerror(genders_errnum(handle));
}

void 
genders_perror(genders_t handle, const char *msg) 
{
  char *errormsg = genders_strerror(genders_errnum(handle));

  if (!msg)
    fprintf(stderr, "%s\n", errormsg);
  else
    fprintf(stderr, "%s: %s\n", msg, errormsg);
}

int 
genders_getnumnodes(genders_t handle) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numnodes;
}

int 
genders_getnumattrs(genders_t handle) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numattrs;
}

int 
genders_getmaxattrs(genders_t handle) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxattrs;
}

int 
genders_getmaxnodelen(genders_t handle) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxnodelen;
}

int 
genders_getmaxattrlen(genders_t handle) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxattrlen;
}

int 
genders_getmaxvallen(genders_t handle) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxvallen;
}

/* 
 * _genders_list_create
 *
 * Generic list create for genders_nodelist_create,
 * genders_attrlist_create, and genders_vallist_create.
 *
 */
static int 
_genders_list_create(genders_t handle, char ***list, int len, int buflen) 
{
  char **templist = NULL;
  int i = 0;

  assert(handle && handle->magic == GENDERS_MAGIC_NUM); 

  if (len > 0) 
    {
      if (!list) 
	{
	  handle->errnum = GENDERS_ERR_PARAMETERS;
	  return -1;
	}

      __xmalloc(templist, char **, sizeof(char *) * len);
      for (i = 0; i < len; i++)
	__xmalloc(templist[i], char *, buflen);
      *list = templist;
    }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return len;

 cleanup:
  if (templist) 
    {
      int j;
      for (j = 0; j < i; j++)
	free(templist[j]);
      free(templist);
    }
  return -1;
}

/* 
 * _genders_list_clear
 *
 * Generic list clear for genders_nodelist_clear,
 * genders_attrlist_clear, and genders_vallist_clear.
 *
 */
static int 
_genders_list_clear(genders_t handle, char **list, int len, int buflen) 
{
  assert(handle && handle->magic == GENDERS_MAGIC_NUM); 

  if (len > 0) 
    {
      int i;
      
      if (!list) 
	{
	  handle->errnum = GENDERS_ERR_PARAMETERS;
	  return -1;
	}
      
      for (i = 0; i < len; i++) 
	{
	  if (!list[i]) 
	    {
	      handle->errnum = GENDERS_ERR_NULLPTR;
	      return -1;
	    }
	  memset(list[i], '\0', buflen);
	}
    }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

/* 
 * _genders_list_destroy
 *
 * Generic list destroy for genders_nodelist_destroy,
 * genders_attrlist_destroy, and genders_vallist_destroy.
 *
 */
static int 
_genders_list_destroy(genders_t handle, char **list, int len) 
{
  assert(handle && handle->magic == GENDERS_MAGIC_NUM); 

  if (len > 0) 
    {
      int i;
      
      if (!list) 
	{
	  handle->errnum = GENDERS_ERR_PARAMETERS;
	  return -1;
	}

      for (i = 0; i < len; i++)
	free(list[i]);
      free(list);
    }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int 
genders_nodelist_create(genders_t handle, char ***list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_create(handle, 
			      list, 
			      handle->numnodes, 
			      handle->maxnodelen+1);
}

int 
genders_nodelist_clear(genders_t handle, char **list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_clear(handle, 
			     list, 
			     handle->numnodes, 
			     handle->maxnodelen+1);
}

int 
genders_nodelist_destroy(genders_t handle, char **list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_destroy(handle, 
			       list, 
			       handle->numnodes);
}

int 
genders_attrlist_create(genders_t handle, char ***list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_create(handle, 
			      list, 
			      handle->numattrs, 
			      handle->maxattrlen+1);
}

int 
genders_attrlist_clear(genders_t handle, char **list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_clear(handle, 
			     list, 
			     handle->numattrs, 
			     handle->maxattrlen+1);
}

int 
genders_attrlist_destroy(genders_t handle, char **list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;
  
  return _genders_list_destroy(handle, 
			       list, 
			       handle->numattrs);
}

int 
genders_vallist_create(genders_t handle, char ***list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_create(handle, 
			      list, 
			      handle->numattrs, 
			      handle->maxvallen+1);
}

int 
genders_vallist_clear(genders_t handle, char **list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_clear(handle, 
			     list, 
			     handle->numattrs, 
			     handle->maxvallen+1);
}

int 
genders_vallist_destroy(genders_t handle, char **list) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  return _genders_list_destroy(handle, 
			       list, 
			       handle->numattrs);
}

int 
genders_getnodename(genders_t handle, char *node, int len) 
{
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  if (!node || len < 0) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      return -1;
    }

  if ((strlen(handle->nodename) + 1) > len) 
    {
      handle->errnum = GENDERS_ERR_OVERFLOW;
      return -1;
    }

  strcpy(node, handle->nodename);
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int 
genders_getnodes(genders_t handle, char *nodes[], int len, 
                 const char *attr, const char *val) 
{
  ListIterator itr = NULL;
  genders_node_t n;
  int index = 0;
  int retval = -1;

  if (_loaded_handle_error_check(handle) < 0)
    goto cleanup;

  if ((!nodes && len > 0) || len < 0) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      goto cleanup;
    }

  if (handle->attrval_index 
      && attr
      && val
      && !strcmp(handle->attrval_index_attr, attr)) 
    {
      /* Case A: Use attrval_index to find nodes */
      List l;
      
      if (!(l = hash_find(handle->attrval_index, val))) 
	{
	  /* No attributes with this value */
	  handle->errnum = GENDERS_ERR_SUCCESS;
	  return 0;
	}

      __list_iterator_create(itr, l);
      while ((n = list_next(itr))) 
	{
	  if (_put_in_array(handle, n->name, nodes, index++, len) < 0)
	    goto cleanup;
	}
    }
  else if (attr) 
    {
      /* Case B: atleast attr input */
      List l;
      
      if (!(l = hash_find(handle->attr_index, attr))) 
	{
	  /* No nodes have this attr */
	  handle->errnum = GENDERS_ERR_SUCCESS;
	  return 0;
	}

      __list_iterator_create(itr, l);
      while ((n = list_next(itr))) 
	{
	  genders_attrval_t av;
	  
	  /* val could be NULL */
	  if (_find_attrval(handle, n, attr, val, &av) < 0)
	    goto cleanup;
	  
	  if (av && _put_in_array(handle, n->name, nodes, index++, len) < 0)
	    goto cleanup;
	}
    }
  else 
    {
      /* Case C: get every node */
      __list_iterator_create(itr, handle->nodeslist);
      while ((n = list_next(itr))) 
	{
	  if (_put_in_array(handle, n->name, nodes, index++, len) < 0)
	    goto cleanup;
	}
    }
  
  retval = index;
  handle->errnum = GENDERS_ERR_SUCCESS;
 cleanup:
  __list_iterator_destroy(itr);
  return retval;
}

int 
genders_getattr(genders_t handle, 
		char *attrs[], 
		char *vals[],
                int len, 
		const char *node) 
{
  ListIterator attrlist_itr = NULL;
  ListIterator attrvals_itr = NULL;
  List attrvals;
  genders_node_t n;
  int index = 0;
  int retval = -1;

  if (_loaded_handle_error_check(handle) < 0)
    goto cleanup;

  if ((!attrs && len > 0) || len < 0) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      goto cleanup;
    }

  if (!node)
    node = handle->nodename;
  
  if (!(n = hash_find(handle->node_index, node))) 
    {
      handle->errnum = GENDERS_ERR_NOTFOUND;
      return -1;
    }

  __list_iterator_create(attrlist_itr, n->attrlist);
  while ((attrvals = list_next(attrlist_itr))) 
    {
      genders_attrval_t av;
      
      __list_iterator_create(attrvals_itr, attrvals);
      while ((av = list_next(attrvals_itr))) 
	{
	  if (_put_in_array(handle, av->attr, attrs, index, len) < 0)
	    goto cleanup;
      
	  if (vals && av->val) 
	    {
	      char *valptr;
	      if (_get_valptr(handle, n, av, &valptr, NULL) < 0)
		goto cleanup;
	      if (_put_in_array(handle, valptr, vals, index, len) < 0)
		goto cleanup;
	    }
	  index++;
	}
      __list_iterator_destroy(attrvals_itr);
    }
  attrvals_itr = NULL;
  
  retval = index;
  handle->errnum = GENDERS_ERR_SUCCESS;
 cleanup:
  __list_iterator_destroy(attrlist_itr);
  __list_iterator_destroy(attrvals_itr);
  return retval;  
}

int 
genders_getattr_all(genders_t handle, char *attrs[], int len) 
{
  ListIterator attrslist_itr = NULL; 
  char *attr;
  int index = 0;
  int retval = -1;
  
  if (_loaded_handle_error_check(handle) < 0)
    goto cleanup;

  if ((!attrs && len > 0) || len < 0) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      goto cleanup;
    }

  if (handle->numattrs > len) 
    {
      handle->errnum = GENDERS_ERR_OVERFLOW;
      goto cleanup;
    }

  __list_iterator_create(attrslist_itr, handle->attrslist);
  while ((attr = list_next(attrslist_itr))) 
    {
      if (_put_in_array(handle, attr, attrs, index++, len) < 0)
	goto cleanup;
    }

  retval = index;
  handle->errnum = GENDERS_ERR_SUCCESS;
 cleanup:
  __list_iterator_destroy(attrslist_itr);
  return retval;  
}

int 
genders_testattr(genders_t handle, 
		 const char *node, 
		 const char *attr, 
                 char *val, 
		 int len) 
{
  genders_node_t n;
  genders_attrval_t av;

  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  if (!attr || (val && len < 0)) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      return -1;
    }

  if (!node)
    node = handle->nodename;

  if (!(n = hash_find(handle->node_index, node))) 
    {
      handle->errnum = GENDERS_ERR_NOTFOUND;
      return -1;
    }

  if (_find_attrval(handle, n, attr, NULL, &av) < 0)
    return -1;

  if (av) 
    {
      if (val) 
	{
	  if (av->val) 
	    {
	      char *valptr;
	      if (_get_valptr(handle, n, av, &valptr, NULL) < 0)
		return -1;
	      if (strlen(valptr + 1) > len) 
		{
		  handle->errnum = GENDERS_ERR_OVERFLOW;
		  return -1;
		}
	      strcpy(val, valptr);
	    }
	  else
	    memset(val, '\0', len);
	}
    }
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return ((av) ? 1 : 0);
}

int 
genders_testattrval(genders_t handle, 
		    const char *node, 
                    const char *attr, 
		    const char *val) 
{
  genders_node_t n;
  genders_attrval_t av;

  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  if (!attr) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      return -1;
    }

  if (!node)
    node = handle->nodename;

  if (!(n = hash_find(handle->node_index, node))) 
    {
      handle->errnum = GENDERS_ERR_NOTFOUND;
      return -1;
    }

  if (_find_attrval(handle, n, attr, val, &av) < 0)
    return -1;
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return ((av) ? 1 : 0);
}

int 
genders_isnode(genders_t handle, const char *node) 
{
  genders_node_t n;

  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  if (!node)
    node = handle->nodename;

  n = hash_find(handle->node_index, node);
  handle->errnum = GENDERS_ERR_SUCCESS;
  return ((n) ? 1 : 0);
}

int 
genders_isattr(genders_t handle, const char *attr) 
{
  void *ptr;
 
  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  if (!attr) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      return -1;
    }

  ptr = hash_find(handle->attr_index, attr);
  handle->errnum = GENDERS_ERR_SUCCESS;
  return ((ptr) ? 1 : 0);
}

int 
genders_isattrval(genders_t handle, const char *attr, const char *val) 
{
  ListIterator nodeslist_itr = NULL;
  genders_node_t n;
  genders_attrval_t av;
  int retval = -1;

  if (_loaded_handle_error_check(handle) < 0)
    goto cleanup;

  if (!attr || !val) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      goto cleanup;
    }
  
  if (handle->attrval_index 
      && !strcmp(handle->attrval_index_attr, attr)) 
    {
      List l;
      
      if (!(l = hash_find(handle->attrval_index, val)))
	retval = 0;
      else
	retval = 1;
      
      handle->errnum = GENDERS_ERR_SUCCESS;
      return retval;
    }
  else 
    {
      __list_iterator_create(nodeslist_itr, handle->nodeslist);
      while ((n = list_next(nodeslist_itr))) 
	{
	  if (_find_attrval(handle, n, attr, val, &av) < 0) 
	    goto cleanup;
	  if (av) 
	    {
	      retval = 1;
	      handle->errnum = GENDERS_ERR_SUCCESS;
	      goto cleanup;
	    }
	}
    }

  retval = 0;
  handle->errnum = GENDERS_ERR_SUCCESS;
 cleanup:
  __list_iterator_destroy(nodeslist_itr);
  return retval;
}

int
genders_index_attrvals(genders_t handle, const char *attr)
{
  ListIterator nodeslist_itr = NULL;
  List l = NULL;
  List attrval_buflist = NULL;
  genders_node_t n;
  char *valbuf = NULL;
  hash_t attrval_index = NULL;
  char *attrval_index_attr = NULL;
  int rv;

  if (_loaded_handle_error_check(handle) < 0)
    return -1;

  if (!attr) 
    {
      handle->errnum = GENDERS_ERR_PARAMETERS;
      goto cleanup;
    }

  if ((rv = genders_isattr(handle, attr)) < 0)
    goto cleanup;

  /* check if attr is legit */
  if (rv == 0) 
    {
      handle->errnum = GENDERS_ERR_NOTFOUND;
      goto cleanup;
    }

  /* check if index already created */
  if (handle->attrval_index && !strcmp(handle->attrval_index_attr, attr)) 
    {
      handle->errnum = GENDERS_ERR_SUCCESS;
      return 0;
    }

  /* Max possible hash size is number of nodes, so pick upper boundary */
  __hash_create(attrval_index, 
                handle->numnodes, 
                (hash_key_f)hash_key_string,
                (hash_cmp_f)strcmp, 
                (hash_del_f)list_destroy);

  /* Create a List to store buffers for later freeing */
  __list_create(attrval_buflist, free);

  __list_iterator_create(nodeslist_itr, handle->nodeslist);
  while ((n = list_next(nodeslist_itr))) 
    {
      int subst_occurred = 0;
      genders_attrval_t av;
      
      if (_find_attrval(handle, n, attr, NULL, &av) < 0)
	goto cleanup;

      if (av) 
	{
	  char *valptr;
	  
	  if (av->val) 
	    {
	      if (_get_valptr(handle, n, av, &valptr, &subst_occurred) < 0)
		goto cleanup;
	    }
	  else
	    valptr = GENDERS_NOVALUE;

	  if (!(l = hash_find(attrval_index, valptr))) 
	    {
	      __list_create(l, NULL);
	      
	      /* If a substitution occurred, we cannot use the av->val
	       * pointer as the key, b/c the key contains some nonsense
	       * characters (i.e. %n).  So we have to copy this buffer and
	       * store it somewhere to be freed later.
	       */
	      if (subst_occurred) 
		{
		  __xstrdup(valbuf, valptr);
		  __list_append(attrval_buflist, valbuf);
		  valptr = valbuf;
		  valbuf = NULL;
		}
	      
	      __hash_insert(attrval_index, valptr, l);
	    }
      
	  __list_append(l, n);
	  l = NULL;
	}
    }

  __xstrdup(attrval_index_attr, attr);

  /* Delete/free previous index */
  __hash_destroy(handle->attrval_index);
  free(handle->attrval_index_attr);
  __list_destroy(handle->attrval_buflist);
  handle->attrval_index = NULL;
  handle->attrval_index_attr = NULL;

  handle->attrval_index = attrval_index;
  handle->attrval_index_attr = attrval_index_attr;
  handle->attrval_buflist = attrval_buflist;

  __list_iterator_destroy(nodeslist_itr);
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
  
 cleanup:
  __list_iterator_destroy(nodeslist_itr);
  __list_destroy(l);
  __hash_destroy(attrval_index);
  __list_destroy(attrval_buflist);
  free(attrval_index_attr);
  free(valbuf);
  return -1;
}

int 
genders_parse(genders_t handle, const char *filename, FILE *stream) 
{
  int errcount, retval = -1;
  List debugnodeslist = NULL;
  List debugattrvalslist = NULL;

  if (_handle_error_check(handle) < 0)
    goto cleanup;

  if (!stream)
    stream = stderr;

  __list_create(debugnodeslist, _free_genders_node);
  __list_create(debugattrvalslist, NULL);

  if ((errcount = genders_open_and_parse(handle, 
					 filename,
					 debugnodeslist, 
					 debugattrvalslist,
					 1, 
					 stream)) < 0)
    goto cleanup;

  retval = errcount;
  handle->errnum = GENDERS_ERR_SUCCESS;
 cleanup:
  __list_destroy(debugnodeslist);
  __list_destroy(debugattrvalslist);
  return retval;
}

void 
genders_set_errnum(genders_t handle, int errnum) 
{
  if (_handle_error_check(handle) < 0)
    return;

  handle->errnum = errnum;
}
