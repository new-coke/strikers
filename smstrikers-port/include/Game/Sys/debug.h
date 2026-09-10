#ifndef _DEBUG_H_
#define _DEBUG_H_

enum eDEBUG_CHANNEL
{
    DC_STARTUP = 0,
    DC_GL = 1,
    DC_GLPLAT = 2,
    DC_RENDER = 3,
    DC_AI = 4,
    DC_CAMERA = 5,
    DC_EVENT = 6,
    DC_FE = 7,
    DC_PHYSICS = 8,
    DC_MEMORY = 9,
    DC_SOUND = 10,
    DC_TASKMAN = 11,
    DC_LOADER = 12,
    DC_MISC = 13,
    DC_REPLAY = 14,
    DC_NIS = 15,
    DC_NETWORK = 16,
    DC_MULTISTREAM = 17,
    DC_WORLD = 18,
    DC_CONFIG_SYSTEM = 19,
    DC_NUM_CHANNELS = 20,
};

namespace tDebugPrintManager
{

int Print(eDEBUG_CHANNEL channel, const char* format, ...);
void Initialize();
bool ParseDebugChannelFile(const char* path);

} // namespace tDebugPrintManager

#endif // _DEBUG_H_
