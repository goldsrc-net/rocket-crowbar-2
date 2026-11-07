// Rocket Crowbar 2: the vote menu (VGUI menu 10, opened by VoteBegin). client.dll keeps no file
// names: this is the run of RC2's code linked between vgui_teammenu and view. Names ours.
#pragma once

#include "vgui_TeamFortressViewport.h"

class CVoteMenuPanel : public CMenuPanel // name ours
{
public:
	CVoteMenuPanel();

	void SetVoteText(const char* pszText); // name ours

private:
	Label* m_pLabel; // 0xd4: what the vote is about
};
