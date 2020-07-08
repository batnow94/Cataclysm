#ifndef _KEYPRESS_H_
#define _KEYPRESS_H_

#ifndef SOCRATES_DAIMON
#include "enums.h"
#include "action.h"
#include "keymap.h"
#endif

// Simple text input--translates numpad to vikeys
int input();
// If ch is vikey, x & y are set to corresponding direction; ch=='y'->x=-1,y=-1
void get_direction(int &x, int &y, int ch);
// Uses the keymap to figure out direction properly
#ifndef SOCRATES_DAIMON
point get_direction(int ch);
#endif

#ifndef SOCRATES_DAIMON
extern keymap<action_id> keys;
#endif

#define CTRL(n) (n - 'A' + 1 < 1 ? n - 'a' + 1 : n - 'A' + 1)
#endif
