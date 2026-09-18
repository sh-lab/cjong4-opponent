#include "cjong4/opponent/opponent_betaori.h"
#include "cjong4/opponent/opponent_standard.h"

int
main(void)
{
    cj4m_player_delegate delegate = cj4_opponent_betaori(1);
    cj4m_player_delegate standard = cj4_opponent_standard(1);
    return delegate.decide && standard.decide ? 0 : 1;
}
