#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct BufferFrame {
    uint32_t channel_id;
    size_t payload_len;
    uint8_t data[]; /* Flexible Array Member: C99+ */
} BufferFrame;

static BufferFrame* frame_create(uint32_t ch, const uint8_t *src, size_t len) {
    BufferFrame *frame = malloc(sizeof *frame + len);
    if (!frame) return NULL;
    frame->channel_id = ch;
    frame->payload_len = len;
    if (src && len > 0) {
        memcpy(frame->data, src, len);
    }
    return frame;
}

int main(void) {
    printf("=== C Flexible Array Members Demo ===\n");
    printf("Header sizeof(BufferFrame): %zu bytes\n", sizeof(BufferFrame));

    const uint8_t sample_bytes[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01};
    BufferFrame *frame = frame_create(4, sample_bytes, 5);
    if (!frame) return 1;

    printf("Frame created: channel=%u, payload_len=%zu\n",
           frame->channel_id, frame->payload_len);
    printf("Payload data: ");
    for (size_t i = 0; i < frame->payload_len; ++i) {
        printf("0x%02X ", frame->data[i]);
    }
    printf("\n");

    free(frame);
    return 0;
}
