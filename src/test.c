/* #define TEST_MODULE_PFD */
/* #define TEST_MODULE_ND */
/* #define TEST_MODULE_EICAS1 */
/* #define TEST_MODULE_EICAS2 */
/* #define TEST_MODULE_FMC */
#define TEST_MODULE_COCKPIT

#ifdef TEST_MODULE_PFD
#include "PFD/pfd_main.h"
#endif

#ifdef TEST_MODULE_ND
#include "ND/nd_main.h"
#endif

#ifdef TEST_MODULE_EICAS1
#include "EICAS1/eicas1_main.h"
#endif

#ifdef TEST_MODULE_EICAS2
#include "EICAS2/eicas2_main.h"
#endif

#ifdef TEST_MODULE_FMC
#include "FMC/fmc_main.h"
#endif

#ifdef TEST_MODULE_COCKPIT
#include "Cockpit/cockpit_main.h"
#endif

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

#ifdef TEST_MODULE_PFD
    return pfd_main_run();
#endif

#ifdef TEST_MODULE_ND
    return nd_main_run();
#endif

#ifdef TEST_MODULE_EICAS1
    return eicas1_main_run();
#endif

#ifdef TEST_MODULE_EICAS2
    return eicas2_main_run();
#endif

#ifdef TEST_MODULE_FMC
    return fmc_main_run();
#endif

#ifdef TEST_MODULE_COCKPIT
    return cockpit_main_run();
#endif

    return 0;
}
