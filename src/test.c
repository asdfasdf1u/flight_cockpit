/* #define TEST_MODULE_PFD */
#define TEST_MODULE_ND
/* #define TEST_MODULE_EICAS */
/* #define TEST_MODULE_FMC */
/* #define TEST_MODULE_COCKPIT */

#ifdef TEST_MODULE_PFD
#include "PFD/pfd_main.h"
#endif

#ifdef TEST_MODULE_ND
#include "ND/nd_main.h"
#endif

#ifdef TEST_MODULE_EICAS
#include "EICAS/eicas_main.h"
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

#ifdef TEST_MODULE_EICAS
    return eicas_main_run();
#endif

#ifdef TEST_MODULE_FMC
    return fmc_main_run();
#endif

#ifdef TEST_MODULE_COCKPIT
    return cockpit_main_run();
#endif

    return 0;
}
