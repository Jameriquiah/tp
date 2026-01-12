#include "host/HostGame.h"

#if defined(TP_HOST_RUN_GAME)
extern "C" void main01(void);
#endif

void HostGameBootstrap() {
#if defined(TP_HOST_RUN_GAME)
    main01();
#endif
}
