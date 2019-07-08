/*
 * Author:         Tim Ruhl
 *
 * Date:           Nov 27, 1995
 *
 * Message module.
 *         Map Saniya's message interface to Panda 2.1 (new message
 *         extensions).
 */


#include "communication.h"	/* Part of the communication module */
#include "panda_message.h"
#include "pan_sys.h"
#include "pan_module.h"
#include "util.h"

#include <string.h>
#include <assert.h>

#define INITIAL_MAP_SIZE 10
#define CHUNK_MAP_SIZE   10

static int initialized;
static int me;

static message_handler_p *map;
static int size, nr;

static int trailer = 0;

/*
 * find_handler:
 *                 Find the entry number corresponding to message handler
 *                 mh.
 */
static int
find_handler(message_handler_p mh)
{
    int i;

    for(i = 0; i < nr; i++) {
	if (map[i] == mh) return i;
    }

    return -1;
}
    

int 
init_message(int moi, int gsize, int pdebug)
{
    int i;

    if (initialized++) return 0;

    me = moi;

    /* Initialize table */
    size = INITIAL_MAP_SIZE;
    map = pan_malloc(sizeof(message_handler_p) * size);
    for(i = 0; i < size; i++) map[i] = NULL;

    return 0;
}

int 
finish_message(void)
{
    if (--initialized) return 0;

    pan_free(map);
    nr = size = -1;
    map = NULL;

    return 0;
}

/*
 * new_message:
 *                 Create a new RTS message. A message contains a pointer
 *                 to a Panda message. The Panda message contains three
 *                 fields:
 *                 - message header (offset 0)
 *                 - user header 
 *                 - user data
 */
message_p 
new_message(int header_size, int max_data_size)
{
    message_p m;

    m = pan_malloc(sizeof(message_t));

    m->size = max_data_size;
    m->data = pan_malloc(sizeof(msg_hdr_t) + header_size + m->size + trailer);
    m->hdr = (msg_hdr_p)m->data;

    /* Fill message header */
    m->hdr->hdr_size = header_size;
    m->hdr->data_size = 0;

    return m;
}

/*
 * free_message:
 *                 Free the rts message
 */
int 
free_message(message_p m)
{
    pan_free(m->data);

#ifndef NDEBUG
    m->data = NULL;
    m->hdr = NULL;
    m->size = -1;
#endif

    pan_free(m);
    
    return 0;
}

/*
 * message_set_header:
 *                 Set the contents of the user header field.
 */
int 
message_set_header(message_p m, void *hdr, int size)
{
    assert(size == m->hdr->hdr_size);

    memcpy(m->data + sizeof(msg_hdr_t), hdr, size);

    return 0;
}

/*
 * message_set_data:
 *                 Set the contents of the data field.
 */
int 
message_set_data(message_p m, void *buffer, int size)
{
    if (size > m->size){
	m->size = size;
	m->data = pan_realloc(m->data, sizeof(msg_hdr_t) + 
			      m->hdr->hdr_size + m->size + trailer);
	m->hdr = (msg_hdr_p)m->data;
    }

    memcpy(m->data + sizeof(msg_hdr_t) + m->hdr->hdr_size, buffer, size);
    m->hdr->data_size = size;

    return 0;
}

/*
 * message_set_handler:
 *                 Set the message handler routine in the message
 *                 header. All handler routines are converted from
 *                 pointer to integer before added to the message.
 */
int 
message_set_handler(message_p m, message_handler_p mh)
{
    int e;

    e = find_handler(mh);
    assert(e != -1);

    m->hdr->msg_handler = e;

    return 0;
}

/*
 * message_duplicate:
 *                 copies 'message' into 'destmessage'.
 */
int
message_duplicate(message_p destmessage, message_p message) {
  message_set_header(destmessage,message_get_header(message),message_get_header_size(message));
  message_set_data(destmessage,message_get_data(message),message_get_data_size(message));
  message_set_handler(destmessage,message_get_handler(message));
  return 0;
}
  
/*
 * message_clear_data:
 *                 Clear all user data from the message.
 */
int 
message_clear_data(message_p m)
{
    m->hdr->data_size = 0;
    
    return 0;
}

/*
 * message_append_data:
 *                 Append a data buffer to the user data part of the
 *                 message. All data appended is consecutive to the
 *                 previous data part.
 */
int 
message_append_data(message_p m, void *buffer, int size)
{
    if (m->hdr->data_size + size > m->size){
	m->size = m->hdr->data_size + size;
	m->data = pan_realloc(m->data, sizeof(msg_hdr_t) + m->hdr->hdr_size + 
			      m->size + trailer);
	m->hdr = (msg_hdr_p)m->data;
    }

    memcpy(m->data + sizeof(msg_hdr_t) + m->hdr->hdr_size + m->hdr->data_size, 
	   buffer, size);
    m->hdr->data_size += size;
    
    return 0;
}

/*
 * message_combine_data:
 *                 Combine the data of two equally sized data buffers.
 */
int 
message_combine_data(message_p m, void *buffer, int size,
		     message_combine_function cf)
{
    assert(size == m->hdr->data_size);

    (*cf)(m->data + sizeof(msg_hdr_t) + m->hdr->hdr_size, buffer);

    return 0;
}

/*
 * message_get_header:
 *                 Get a pointer to the user header.
 */
void *
message_get_header(message_p m)
{
    return m->data + sizeof(msg_hdr_t);
}

/*
 * message_get_header_size:
 *		   Returns the size of the user header.
 */
int
message_get_header_size(message_p m)
{
    return m->hdr->hdr_size;
}

/*
 * message_get_data_size:
 *                 Returns the size of the user data part.
 */
int 
message_get_data_size(message_p m)
{
    return m->hdr->data_size;
}

/*
 * message_get_data:
 *                 Returns a pointer to the user data part.
 */
void *
message_get_data(message_p m)
{
    return m->data + sizeof(msg_hdr_t) + m->hdr->hdr_size;
}

/*
 * message_copy_data:
 *                 Copies the contents of the user data buffer to buffer.
 */
int 
message_copy_data(message_p m, void *buffer, int buffer_size)
{
    assert(buffer_size == m->hdr->data_size);

    memcpy(buffer, m->data + sizeof(msg_hdr_t) + m->hdr->hdr_size, 
	   buffer_size);

    return 0;
}

/*
 * message_get_handler:
 *                 Returns a pointer to the handler function associated
 *                 with the message.
 */
message_handler_p 
message_get_handler(message_p m)
{
    int e;

    e = m->hdr->msg_handler;

    assert(map[e]);
    return map[e];
}

/*
 * message_register_handler:
 *                 Register a handler function. A mapping is created from
 *                 function pointer to index. This index is used in the
 *                 message header.
 */
int 
message_register_handler(message_handler_p mh)
{
    int i, e;

    if (nr >= size) {
	assert(nr == size);
	
	size += CHUNK_MAP_SIZE;
	map = pan_realloc(map, sizeof(message_handler_p) * size);

	for(i = nr; i < size; i++) map[i] = NULL;
    }

    e = nr++;
    assert(e >= 0 && e < size);
    assert(map[e] == NULL);

    map[e] = mh;

    return e;
}

/*
 * message_unregister_handler:
 *                 Unregister a handler function.
 */
int 
message_unregister_handler(message_handler_p mh)
{
    int e;

    e = find_handler(mh);
    if (e == -1) return -1;

    map[e] = NULL;

    return 0;
}

/*
 * rts_message_receive:
 *                 Converts a Panda message to an RTS message.
 */
message_p
rts_message_receive(void *data, int size, int len)
{
    message_p m;

    m = pan_malloc(sizeof(message_t));

    m->data = data;
    m->hdr = (msg_hdr_p)m->data;
    m->size = size - (sizeof(msg_hdr_t) + m->hdr->hdr_size + trailer);
    assert(m->hdr->data_size == len - (sizeof(msg_hdr_t) + m->hdr->hdr_size));

    return m;
}

/* rts_message_trailer:
 *                 Register a trailer size to the message module. The
 *                 message module guarantees that all messages have the
 *                 lart part of the message data unused. The size of this
 *                 part is the maximum of all registered trailers.
 */
void
rts_message_trailer(int t)
{
    if (t > trailer) trailer = t;
}

/* rts_message_length:
 *                 Returns the size of the message that has to be
 *                 sent. This is the sum of the communication header, the
 *                 user header, and the user data.
 */
int
rts_message_length(message_p m)
{
    return sizeof(msg_hdr_t) + m->hdr->hdr_size + m->hdr->data_size;
}
