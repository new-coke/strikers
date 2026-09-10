// clang devirtualises m_data.GetID() to EventData::GetID, which returns -1.

#include "Game/Sys/eventman.h"

__attribute__((noinline)) u32 port_event_data_id(EventData* pData) { return pData->GetID(); }
