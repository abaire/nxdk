#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>



//asdfasdf
int main(void)
{
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    struct LaunchData00000001 {
        uint32_t reason;
        uint32_t context;
        uint32_t parameters[2];
        uint8_t padding[3072 - 16];
    };

    struct LaunchData00000001 launch_data = {
        6,
        0,
        {0, 0},
        {0}
     };

    XLaunchXBEEx(NULL, &launch_data);

    while(1) {
        debugPrint("Hello nxdk!\n");
        Sleep(2000);
    }

    return 0;
}
