#ifndef WOW_C_SYSTEM_H_
#define WOW_C_SYSTEM_H_

#include "wow/c/types.h"

#ifdef __cplusplus
extern "C" {
#endif

WowResult WowInit(void);
void WowShutdown(void);

uint32_t WowOrdinal(const char* name);
WowResult WowSetPack(int pack);
int WowIsWin32Ordinal(uint32_t ordinal);

WowResult WowOccupyWin32(const char* label);
WowResult WowOccupyCatalog(const char* label);
WowResult WowOpen(int kind, const char* label, int parent, int* out_id);
WowResult WowWrite(int id, const void* bytes, uint32_t len);
WowResult WowRead(int id, void* buf, uint32_t len, uint32_t* out_len);
WowResult WowMap(int id);
WowResult WowUnmap(int id);

WowResult WowStatus(char* buf, uint32_t len);
WowResult WowThinMap(char* buf, uint32_t len);
WowResult WowLastEvent(char* buf, uint32_t len);
WowResult WowLastError(char* buf, uint32_t len);
WowResult WowCall(const char* topic, const char* payload, char* buf,
                  uint32_t len);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WOW_C_SYSTEM_H_
