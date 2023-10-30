// procui implementation based on <https://github.com/devkitPro/wut/blob/master/libraries/libwhb/src/proc.c>
// we need to use a custom one to handle home button input properly

#include "3ds.h"

#include <coreinit/foreground.h>
#include <coreinit/title.h>
#include <coreinit/systeminfo.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>

#define HBL_TITLE_ID (0x0005000013374842)
#define MII_MAKER_JPN_TITLE_ID (0x000500101004A000)
#define MII_MAKER_USA_TITLE_ID (0x000500101004A100)
#define MII_MAKER_EUR_TITLE_ID (0x000500101004A200)

static int running = 0;
static int fromHBL = 0;
static int homeEnabled = 1;

static uint32_t procSaveCallback(void* context)
{
   OSSavesDone_ReadyToRelease();
   return 0;
}

static uint32_t procHomeButtonDenied(void* context)
{
    if (fromHBL && homeEnabled) {
        n3ds_proc_stop_running();
    }

    return 0;
}

void n3ds_proc_init(void)
{
    uint64_t titleID = OSGetTitleID();
    if (titleID == HBL_TITLE_ID ||
        titleID == MII_MAKER_JPN_TITLE_ID ||
        titleID == MII_MAKER_USA_TITLE_ID ||
        titleID == MII_MAKER_EUR_TITLE_ID) {
        fromHBL = 1;
        OSEnableHomeButtonMenu(FALSE);
    }

    running = 1;
    ProcUIInitEx(&procSaveCallback, NULL);

    n3ds_proc_register_home_callback();
}

void n3ds_proc_shutdown(void)
{
    running = 0;

    // If we're running from Homebrew Launcher we must do a SYSRelaunchTitle to
    // correctly return to HBL.
    if (fromHBL) {
        SYSRelaunchTitle(0, NULL);
    }
}

void n3ds_proc_register_home_callback(void)
{
    if (fromHBL) {
        ProcUIRegisterCallback(PROCUI_CALLBACK_HOME_BUTTON_DENIED, &procHomeButtonDenied, NULL, 100);
    }
}

int n3ds_proc_running(void)
{
    ProcUIStatus status;

    status = ProcUIProcessMessages(TRUE);
    if (status == PROCUI_STATUS_EXITING) {
        running = false;
    }
    else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
        ProcUIDrawDoneRelease();
    }

    if (!running) {
        ProcUIShutdown();
    }

    return running;
}

void n3ds_proc_stop_running(void)
{
    if (fromHBL) {
        running = false;
    }
    else {
        SYSLaunchMenu();
    }
}

void n3ds_proc_set_home_enabled(int enabled)
{
    if (fromHBL) {
        homeEnabled = enabled;
    }
    else {
        OSEnableHomeButtonMenu(enabled);
    }
}
