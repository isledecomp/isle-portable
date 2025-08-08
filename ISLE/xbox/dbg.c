#include <xboxkrnl/xboxkrnl.h>
#include <SDL3/SDL.h>


void my_output(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    DbgPrint("%s\n", message);
}
