#include "cabin_api.h"
#include "cabin_main.h"
#include "../Data/sim_data_center.h"

#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static DWORD WINAPI push_escape_events(LPVOID user_data)
{
    (void)user_data;
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_ESCAPE;

    Sleep(150);
    (void)SDL_PushEvent(&event);
    Sleep(350);
    (void)SDL_PushEvent(&event);
    return 0;
}

static int run_with_escape_events(int (*run)(void))
{
    HANDLE event_thread = CreateThread(NULL, 0, push_escape_events, NULL, 0, NULL);
    assert(event_thread != NULL);
    const int result = run();
    WaitForSingleObject(event_thread, INFINITE);
    CloseHandle(event_thread);
    return result;
}

static SimDataCenter *g_shared_center;

static int run_shared_center(void)
{
    return cabin_main_run_with_sim_data_center(g_shared_center);
}

int main(int argc, char **argv)
{
    SimDataCenter center;
    (void)argc;
    (void)argv;

    cabin_api_set_key(NULL, 0);
    assert(run_with_escape_events(cabin_main_run) == 0);
    assert(run_with_escape_events(cabin_main_run) == 0);

    assert(sim_data_center_init(&center));
    g_shared_center = &center;
    assert(run_with_escape_events(run_shared_center) == 0);
    g_shared_center = NULL;
    sim_data_center_destroy(&center);

    printf("Cabin lifecycle tests passed.\n");
    return 0;
}
