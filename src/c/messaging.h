#pragma once
#include <pebble.h>

// cold = "the watch has no data for a placed widget": tells the phone to bypass its
// per-source throttles and re-send even unchanged values.
void messaging_requestNewWeatherData(bool cold);

void messaging_init(void (*message_processed_callback)(void));
void inbox_received_callback(DictionaryIterator *iterator, void *context);
void inbox_dropped_callback(AppMessageResult reason, void *context);
void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context);
void outbox_sent_callback(DictionaryIterator *iterator, void *context);
