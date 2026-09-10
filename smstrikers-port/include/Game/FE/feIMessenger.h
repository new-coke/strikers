#ifndef _FEIMESSENGER_H_
#define _FEIMESSENGER_H_

#include "NL/nlFunction.h"
#include "NL/nlBasicString.h"

class FEIMessenger
{
public:
    virtual void OpenMessenger() = 0;
    virtual void OpenMessengerNow() = 0;
    // virtual void SetDisplayMessage(const char*) = 0;
    virtual void SetDisplayMessage(const BasicString<unsigned short, Detail::TempStringAllocator>&) = 0;
    virtual void SetMessageFinishedCB(const Function<FnVoidVoid>& cb) = 0;
    virtual void CloseMessenger() = 0;
    virtual void CloseMessengerNow() = 0;
    virtual bool IsMessengerOpen() const = 0;

    // no members
};

#endif // _FEIMESSENGER_H_
