#pragma once
/**
 * @file psram_allocator.h
 * @brief ArduinoJson allocator that keeps documents in PSRAM.
 *
 * Without it, a large document's small blocks land in internal RAM: the 711 KB material DB
 * once drove internal MinFree to 1.2 KB at boot. Use for any material-DB-sized document.
 */
#include <ArduinoJson.h>

ArduinoJson::Allocator* psramJsonAllocator();
