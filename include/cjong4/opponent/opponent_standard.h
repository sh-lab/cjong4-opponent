#ifndef CJ4M_OPPONENT_STANDARD_H
#define CJ4M_OPPONENT_STANDARD_H

#include "cjong4/manager/delegate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed policy; ctx_level is reserved and does not change the rules. */
cj4m_player_delegate cj4_opponent_standard(int ctx_level);

#ifdef __cplusplus
}
#endif

#endif /* CJ4M_OPPONENT_STANDARD_H */
