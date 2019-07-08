/*=====================================================================*/
/*==== fifo.c : implementation of queue functions                  ====*/
/*==                                                                 ==*/

#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include "fifo.h"

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/* 
 @ create_job_q() : Create the job_queue 
 */

PUB Boolean create_job_q(                  /* Out : enough memory? */
Job_queue_p	job_q)			   /* Out : the job to be created */
{
    job_q->head = NULL;
    job_q->tail = NULL;
    job_q->nr_elts = 0;
    
    return TRUE;
}
	

/*  
 @ add_element : Add the element at the tail of the queue
 */

PUB Boolean add_element(                  /* Out : enough memeory? */
Job_queue_p 	job_q,                    /* Out : the jobqueue */
Edge_t       	elt)                      /* In  : The element to be     
                                           *       added at the tail of  
			  		   *       the queue */
{
    Job_queue_node_p node;
    
    if ((node = (Job_queue_node_p)malloc(sizeof(Job_queue_node_t))) 
              == (Job_queue_node_p)NULL){
	return FALSE;
    }

    copy_edge(&(node->elt), &elt);
    node->next = NULL;
    
    if (job_q->nr_elts == 0)
        job_q->head = node;
    else
	job_q->tail->next = node;

    job_q->tail = node;
    job_q->nr_elts++;
    
    return TRUE;
}
	

/* 
 @ delete_element() : Get the first element of the queue. Don't delete, but update
                      the head
 */

PUB Edge_p delete_element(
Job_queue_p  	 job_q)			   /* Out : The jobqueue          */
{
    Job_queue_node_p      old_head;

    assert (job_q->nr_elts != 0);

    old_head = job_q->head;
    job_q->head = job_q->head->next;
    job_q->nr_elts--;

    return (Edge_p)old_head;
}
    

/* 
 @ delete_job_q() : Delete all elements of the job_queue and the queue itself
 */

PUB void delete_job_q(
Job_queue_p	job_q)                    /* Out : the job to be deleted */
{
    int 	i;
    Job_queue_node_p old_node;

    for(i=1; i <= job_q->nr_elts; i++){
	old_node = job_q->head;
	job_q->head = job_q->head->next;
	free((char*)old_node);
    }
}
	

/*
 @ get_elt_info() : get info on element in the queue
 */

Boolean get_elt_info(
Job_queue_t     job_q,                     /* In  : the queue */
int             index,                     /* In  : position in queue */
Edge_p          *edge)                     /* Out : pointer to edge on position index */
{
    Job_queue_node_p ptr;
    int              cnt;

    if ((index < 1) || (index > job_q.nr_elts))
        return FALSE;

    ptr = job_q.head;
    for (cnt = 1; cnt < index ; cnt++){
        ptr = ptr->next;
    }

    *edge = (Edge_p)ptr;

    return TRUE;
}
    
/*
 @ get_job() : get first job from job queue
 */

PUB Edge_t get_job(			   /* Out : copy of the element */
Job_queue_t	job_q                      /* In  : the job queue */
){
    assert (job_q.nr_elts != 0);

    return (job_q.head->elt);
}


/* 
 @ nr_of_elts() : return the number of elements in the queue
 */

PUB int nr_of_elts(                       /* Out : nr of elements */
Job_queue_t 	job_q)                    /* In : the job_queue  */
{
    return job_q.nr_elts;
}


/*
 @ is_in_queue() :  check if element is in the queue 
 */

PUB Boolean is_in_queue(                 /* Out : is edge in the queue ? */
Job_queue_t 	job_q,                   /* In  : the queue */
Edge_p      	edge)                    /* In  : the edge  */
{
    Job_queue_node_p    ptr;

    if (job_q.nr_elts == 0)
	return FALSE;

    ptr = job_q.head;

    while (ptr != NULL){
	if (equal_edges(&(ptr->elt), edge)){
	    return TRUE;
	}
	ptr = ptr->next;
    }

    return FALSE;
}

        
