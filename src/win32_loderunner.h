#ifndef WIN32_LODERUNNER_H
#define WIN32_LODERUNNER_H


struct win32_game_code
{
    HMODULE GameCodeDLL;
    game_update_and_render *UpdateAndRender;
    bool32 IsValid;
};


#endif