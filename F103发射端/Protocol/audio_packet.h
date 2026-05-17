#ifndef AUDIO_PACKET_H
#define AUDIO_PACKET_H

#include "audio_defs.h"

void audio_packet_make(uint8_t *payload, uint8_t seq, const uint8_t *pcm);

#endif /* AUDIO_PACKET_H */
