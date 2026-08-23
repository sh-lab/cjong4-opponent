#include "cjong4/opponent/opponent_betaori.h"

int
main(void)
{
    cj4m_player_delegate delegate = cj4_opponent_betaori(1);
    return delegate.decide ? 0 : 1;
}
