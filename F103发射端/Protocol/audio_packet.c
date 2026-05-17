#include "audio_packet.h"

#include <string.h>

void audio_packet_make(uint8_t *payload, uint8_t seq, const uint8_t *pcm)
{
    payload[0] = AUDIO_PACKET_TYPE;
    payload[1] = seq;
    memcpy(&payload[2], pcm, AUDIO_SAMPLES_PER_PKT);
}
