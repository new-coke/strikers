#ifndef _EVENTDATA_H_
#define _EVENTDATA_H_

#include "Game/Sys/eventman.h"

template <typename T>
inline void Event::GetData(T** ppData)
{
    s32 id = port_event_data_id(&m_data);
    if (id == -1)
    {
        nlPrintf("Error: Trying to get event data on event with none!\n");
        *ppData = NULL;
    }
    else
    {
        T data;
        id = port_event_data_id(&m_data);
        if (id != (s32)data.GetID())
        {
            nlPrintf("Error: GetData() failed! Data types do not match!\n");
            *ppData = NULL;
        }
        else
        {
            *ppData = (T*)&m_data;
        }
    }
}

#endif // _EVENTDATA_H_
