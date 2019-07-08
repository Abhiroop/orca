/*=====================================================================*/
/*==== fifo.h : queue data structure and function prototypes       ====*/
/*==                                                                 ==*/

/* implementation of fifo queue */

#ifndef fifo_h
#define fifo_h

#include "types.h"
#include "edge.h"

/*---------------------------------------------------------------------*/
/*---- types                                                       ----*/
/*---------------------------------------------------------------------*/


/* the queue */


typedef struct node_struct{
    Edge_t            elt;			/* the info stored     */
    struct  node_struct *next;			/* pointer to next elt */
} Job_queue_node_t;

typedef Job_queue_node_t *Job_queue_node_p;

typedef struct {
    Job_queue_node_p head, tail;                /* pointers to head and tail   */
    int              nr_elts;                   /* number of elements          */
}  Job_queue_t;


typedef Job_queue_t *Job_queue_p;

/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PUB Boolean create_job_q(                  /* Out : enough memory? */
Job_queue_p	job_q);			   /* Out : the job to be created */

PUB Boolean add_element(                   /* Out : enough memeory? */
Job_queue_p     job_q,                     /* Out : the jobqueue */
Edge_t       	elt);                      /* In  : The element to be     
                                            *       added at the tail of  
	      	                            *       the queue */

PUB Boolean is_in_queue(                   /* Out : is edge in the queue ? */
Job_queue_t     job_q,                     /* In  : the queue */
Edge_p          edge);                     /* In  : the edge */


/*
 @ return and delete first element of queue. queue should be non-empty 
 */
PUB Edge_p delete_element(
Job_queue_p  	job_q);                    /* Out : The jobqueue */

PUB void delete_job_q(
Job_queue_p  	job_q);                    /* Out : the job to be deleted */


PUB Boolean get_elt_info(
Job_queue_t     job_q,                     /* In  : the queue */
int             index,                     /* In  : position in queue */
Edge_p          *edge);                    /* Out : pointer to edge on position index */

/* 
 @ get_job(): get first element of the queue, without removing it. queue should be non-empty
 */
 
PUB Edge_t get_job(	   		   /* Out : copy of the element */
Job_queue_t 	job_q);			   /* In  : the job queue */


PUB int nr_of_elts(		  	   /* Out: nr of elements */
Job_queue_t	job_q);                    /* In : the job_queue */

#endif /* fifo_h */
