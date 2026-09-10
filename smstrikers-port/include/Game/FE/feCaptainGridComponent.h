#ifndef _FECAPTAINGRIDCOMPONENT_H_
#define _FECAPTAINGRIDCOMPONENT_H_
#include "Game/FE/feInput.h"
#include "Game/FE/feGridComponent.h"
#include "Game/FE/feMapMenu.h"

#include "Game/Team.h"

class TLComponentInstance;
class TLInstance;
class TLSlide;
struct InlineHasher;

class ICaptainGridComponent : public IGridComponent<eTeamID>
{
public:
    void SetAllItemsActive();
    virtual void MoveHighlightToTarget(eTeamID teamID);
    eTeamID GetSelectedItem() const;
    bool IsValid(eTeamID teamID);
    void SetValid(eTeamID teamID, bool valid);
    void UpdateSuperTeamIconState();
    void Update(eFEINPUT_PAD pad);
    void RebuildInstanceTable();
    void BuildMapMenu();
    ~ICaptainGridComponent();
    ICaptainGridComponent(TLComponentInstance* parentcomponent, bool ismirrored);
};

#endif // _FECAPTAINGRIDCOMPONENT_H_
