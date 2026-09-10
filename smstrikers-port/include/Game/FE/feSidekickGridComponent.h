#ifndef _FESIDEKICKGRIDCOMPONENT_H_
#define _FESIDEKICKGRIDCOMPONENT_H_
#include "Game/FE/feGridComponent.h"

class ISidekickGridComponent : public IGridComponent<eSidekickID>
{
public:
    void SetVisibleInstanceTable(bool visible);
    void MoveHighlightToTarget(eSidekickID id);
    eSidekickID GetSelectedItem() const;
    void Update(eFEINPUT_PAD pad);
    void RebuildInstanceTable();
    void BuildMapMenu();
    ~ISidekickGridComponent();
    ISidekickGridComponent(TLComponentInstance* parentcomponent, bool ismirrored);
};

#endif // _FESIDEKICKGRIDCOMPONENT_H_
