#ifndef PSP_LINK_FILE_RECEIVER_H
#define PSP_LINK_FILE_RECEIVER_H
#include "link_protocol.h"

enum { FILE_BAD_PACKET=1, FILE_NO_SESSION, FILE_BAD_OFFSET, FILE_IO_ERROR,
       FILE_BAD_CHECKSUM, FILE_BUSY };

typedef struct {
    int (*begin)(void *context);
    int (*write)(void *context, const void *data, unsigned length);
    int (*commit)(void *context);
    void (*abort)(void *context);
} link_file_ops_t;

typedef struct {
    uint32_t session, size, expected_crc, offset, crc;
    int active, have_reply;
    link_frame_t last_request, last_reply;
} link_file_receiver_t;

static inline void link_file_abort(link_file_receiver_t *r, const link_file_ops_t *ops, void *ctx)
{
    if (r->active) ops->abort(ctx);
    memset(r, 0, sizeof(*r));
}

/* Return an ACK only after data is accepted by IO; failed/incomplete transfers
 * never commit. Cache the exact last request so lost ACK retries do not append
 * the same bytes twice or recommit an already completed file. */
static inline void link_file_receive(link_file_receiver_t *r, const link_file_ops_t *ops,
                                     void *ctx, const link_frame_t *request, link_frame_t *reply)
{
    unsigned error = 0;
    if (r->have_reply && !memcmp(request, &r->last_request, sizeof(*request))) {
        *reply = r->last_reply;
        return;
    }
    link_init(reply, LINK_FILE_ACK, 0, request->seq, NULL);
    if (request->type == LINK_FILE_BEGIN) {
        if (r->active) error = FILE_BUSY;
        else if (request->length != 8 || !link_get_u32(request->payload) ||
                 link_get_u32(request->payload) > LINK_FILE_LIMIT) error = FILE_BAD_PACKET;
        else if (ops->begin(ctx) < 0) error = FILE_IO_ERROR;
        else {
            r->session = request->seq;
            r->size = link_get_u32(request->payload);
            r->expected_crc = link_get_u32(request->payload + 4);
            r->offset = 0;
            r->crc = UINT32_MAX;
            r->active = 1;
        }
    } else if (!r->active || request->reply_to != r->session) error = FILE_NO_SESSION;
    else if (request->type == LINK_FILE_DATA) {
        if (request->length < 5 || request->length > LINK_PAYLOAD_SIZE) error = FILE_BAD_PACKET;
        else if (link_get_u32(request->payload) != r->offset ||
                 request->length - 4u > r->size - r->offset) error = FILE_BAD_OFFSET;
        else if (ops->write(ctx, request->payload + 4, request->length - 4) < 0) error = FILE_IO_ERROR;
        else {
            r->crc = link_crc_update(r->crc, request->payload + 4, request->length - 4);
            r->offset += request->length - 4;
        }
    } else if (request->type == LINK_FILE_END) {
        if (request->length != 8 || r->offset != r->size ||
            link_get_u32(request->payload) != r->size ||
            link_get_u32(request->payload + 4) != r->expected_crc ||
            (r->crc ^ UINT32_MAX) != r->expected_crc) error = FILE_BAD_CHECKSUM;
        else if (ops->commit(ctx) < 0) error = FILE_IO_ERROR;
        else r->active = 0;
    } else if (request->type == LINK_FILE_ABORT) {
        ops->abort(ctx);
        r->active = 0;
    } else error = FILE_BAD_PACKET;
    if (error) {
        /* A malformed/out-of-order request is a terminal error for this file.
         * Already saved final files are untouched by abort(). */
        if (r->active) ops->abort(ctx);
        r->active = 0;
        reply->type = LINK_FILE_ERROR;
    }
    reply->length = 8;
    link_put_u32(reply->payload, r->offset);
    link_put_u32(reply->payload + 4, error);
    r->last_request = *request;
    r->last_reply = *reply;
    r->have_reply = 1;
}
#endif
