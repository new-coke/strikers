#ifndef _WALKHELPER_H_
#define _WALKHELPER_H_

template <typename KeyType, typename EntryType, typename CallbackType>
class WalkHelper
{
public:
    CallbackType* m_CBClass;
    void (CallbackType::*m_CB)(KeyType*);
    void Callback(EntryType*);
};

template <typename KeyType, typename EntryType, typename CallbackType>
void WalkHelper<KeyType, EntryType, CallbackType>::Callback(EntryType* listEntry)
{
    (m_CBClass->*m_CB)(&listEntry->entry);
}

#endif // _WALKHELPER_H_
