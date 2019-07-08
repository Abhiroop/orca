#include "reduction_function.h"
#include "map.h"
#include "assert.h"

static int initialized=0;
static map_p reduction_function_map;


/*=======================================================================*/
/* Initializes the module. */
/*=======================================================================*/
int 
init_reduction_function(void) {
  if (initialized++) return 0;

  reduction_function_map=new_map();
  assert(reduction_function_map!=NULL);
  return 0;
}

/*=======================================================================*/
/* Finishes the module. */
 
int 
finish_reduction_function(void)
{
  if (--initialized) return 0;
 
  free_map(reduction_function_map);
  return 0;
}
 
/*=======================================================================*/
/* Registers a reduction function into the function table. */
 
int 
register_reduction_function(reduction_function_p rf) {
  return insert_item(reduction_function_map,(item_p)rf);
}
 

/*=======================================================================*/
/* Removes a reduction function from the reduction function table. */
 
int 
release_reduction_function_p(reduction_function_p rf) {
  return remove_item_p(reduction_function_map,rf);
}
 
 
/*=======================================================================*/
/* Removes a reduction function from the reduction function table
   given the function index. */
 
int 
release_reduction_function(int rf) {
  return remove_item(reduction_function_map,rf);
  return 0;
}
 
 
/*=======================================================================*/
/* Returns the (unique) index of a reduction function. */

int 
function_index(reduction_function_p rf) {
  return item_index(reduction_function_map,(item_p)rf);
}

/*=======================================================================*/
/* Returns the pointer to a reduction function given its index. */

reduction_function_p 
function_pointer(int rf) {
  if (rf < 0) return NULL;
  return (reduction_function_p)get_item(reduction_function_map,rf);
}


