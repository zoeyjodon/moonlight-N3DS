#include "n3ds.h"

#include <malloc.h>

#include <vpad/input.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <coreinit/time.h>
#include <coreinit/alarm.h>

#define millis() OSTicksToMilliseconds(OSGetTime())

int disable_gamepad = 0;
int swap_buttons = 0;
int absolute_positioning = 0;

static char lastTouched = 0;
static char touched = 0;

static uint16_t last_x = 0;
static uint16_t last_y = 0;

#define TAP_MILLIS 100
#define DRAG_DISTANCE 10
static uint64_t touchDownMillis = 0;

#define TOUCH_WIDTH 1280
#define TOUCH_HEIGHT 720

static int thread_running;
static OSThread inputThread;
static OSAlarm inputAlarm;

// ~60 Hz
#define INPUT_UPDATE_RATE OSMillisecondsToTicks(16)

void handleTouch(VPADTouchData touch) {
  if (absolute_positioning) {
    if (touch.touched) {
      LiSendMousePositionEvent(touch.x, touch.y, TOUCH_WIDTH, TOUCH_HEIGHT);

      if (!touched) {
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
        touched = 1;
      }
    }
    else if (touched) {
      LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
      touched = 0;
    }
  }
  else {
    // Just pressed (run this twice to allow touch position to settle)
    if (lastTouched < 2 && touch.touched) {
      touchDownMillis = millis();
      last_x = touch.x;
      last_y = touch.y;

      lastTouched++;
      return; // We can't do much until we wait for a few hundred milliseconds
              // since we don't know if it's a tap, a tap-and-hold, or a drag
    }

    // Just released
    if (lastTouched && !touch.touched) {
      if (millis() - touchDownMillis < TAP_MILLIS) {
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
      }
    }

    if (touch.touched) {
      // Holding & dragging screen, not just tapping
      if (millis() - touchDownMillis > TAP_MILLIS || touchDownMillis == 0) {
        if (touch.x != last_x || touch.y != last_y) // Don't send extra data if we don't need to
          LiSendMouseMoveEvent(touch.x - last_x, touch.y - last_y);
        last_x = touch.x;
        last_y = touch.y;
      } else {
        if (touch.x - last_x < -10 || touch.x - last_x > 10) touchDownMillis=0;
        if (touch.y - last_y < -10 || touch.y - last_y > 10) touchDownMillis=0;
        int16_t diff_x = touch.x - last_x;
        int16_t diff_y = touch.y - last_y;
        if (diff_x < 0) diff_x = -diff_x;
        if (diff_y < 0) diff_y = -diff_y;
        if (diff_x + diff_y > DRAG_DISTANCE) touchDownMillis = 0;
      }
    }

    lastTouched = touch.touched ? lastTouched : 0; // Keep value unless released
  }
}

void n3ds_input_init(void)
{
	KPADInit();
	WPADEnableURCC(1);
}

void n3ds_input_update(void) {
  static uint64_t home_pressed[4] = {0};

  short controllerNumber = 0;
  short gamepad_mask = 0;

  for (int i = 0; i < n3ds_input_num_controllers(); i++)
    gamepad_mask |= 1 << i;

  VPADStatus vpad;
  VPADReadError err;
  VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
  if (err == VPAD_READ_SUCCESS && !disable_gamepad) {
    uint32_t btns = vpad.hold;
    short buttonFlags = 0;
#define CHECKBTN(v, f) if (btns & v) buttonFlags |= f;
    if (swap_buttons) {
      CHECKBTN(VPAD_BUTTON_A,       B_FLAG);
      CHECKBTN(VPAD_BUTTON_B,       A_FLAG);
      CHECKBTN(VPAD_BUTTON_X,       Y_FLAG);
      CHECKBTN(VPAD_BUTTON_Y,       X_FLAG);
    }
    else {
      CHECKBTN(VPAD_BUTTON_A,       A_FLAG);
      CHECKBTN(VPAD_BUTTON_B,       B_FLAG);
      CHECKBTN(VPAD_BUTTON_X,       X_FLAG);
      CHECKBTN(VPAD_BUTTON_Y,       Y_FLAG);
    }
    CHECKBTN(VPAD_BUTTON_UP,      UP_FLAG);
    CHECKBTN(VPAD_BUTTON_DOWN,    DOWN_FLAG);
    CHECKBTN(VPAD_BUTTON_LEFT,    LEFT_FLAG);
    CHECKBTN(VPAD_BUTTON_RIGHT,   RIGHT_FLAG);
    CHECKBTN(VPAD_BUTTON_L,       LB_FLAG);
    CHECKBTN(VPAD_BUTTON_R,       RB_FLAG);
    CHECKBTN(VPAD_BUTTON_STICK_L, LS_CLK_FLAG);
    CHECKBTN(VPAD_BUTTON_STICK_R, RS_CLK_FLAG);
    CHECKBTN(VPAD_BUTTON_PLUS,    PLAY_FLAG);
    CHECKBTN(VPAD_BUTTON_MINUS,   BACK_FLAG);
    CHECKBTN(VPAD_BUTTON_HOME,    SPECIAL_FLAG);
#undef CHECKBTN

    // If the button was just pressed, reset to current time
    if (vpad.trigger & VPAD_BUTTON_HOME) home_pressed[controllerNumber] = millis();

    if (btns & VPAD_BUTTON_HOME && millis() - home_pressed[controllerNumber] > 3000) {
      state = STATE_STOP_STREAM;
      return;
    }

    LiSendMultiControllerEvent(controllerNumber++, gamepad_mask, buttonFlags,
      (vpad.hold & VPAD_BUTTON_ZL) ? 0xFF : 0,
      (vpad.hold & VPAD_BUTTON_ZR) ? 0xFF : 0,
      vpad.leftStick.x * INT16_MAX, vpad.leftStick.y * INT16_MAX,
      vpad.rightStick.x * INT16_MAX, vpad.rightStick.y * INT16_MAX);

    VPADTouchData touch;
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touch, &vpad.tpNormal);
    handleTouch(touch);
  }

  KPADStatus kpad_data = {0};
	int32_t kpad_err = -1;
	for (int i = 0; i < 4; i++) {
		KPADReadEx((KPADChan) i, &kpad_data, 1, &kpad_err);
		if (kpad_err == KPAD_ERROR_OK && controllerNumber < 4) {
      if (kpad_data.extensionType == WPAD_EXT_PRO_CONTROLLER) {
        uint32_t btns = kpad_data.pro.hold;
        short buttonFlags = 0;
#define CHECKBTN(v, f) if (btns & v) buttonFlags |= f;
        if (swap_buttons) {
          CHECKBTN(WPAD_PRO_BUTTON_A,       B_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_B,       A_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_X,       Y_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_Y,       X_FLAG);
        }
        else {
          CHECKBTN(WPAD_PRO_BUTTON_A,       A_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_B,       B_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_X,       X_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_Y,       Y_FLAG);
        }
        CHECKBTN(WPAD_PRO_BUTTON_UP,      UP_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_DOWN,    DOWN_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_LEFT,    LEFT_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_RIGHT,   RIGHT_FLAG);
        CHECKBTN(WPAD_PRO_TRIGGER_L,      LB_FLAG);
        CHECKBTN(WPAD_PRO_TRIGGER_R,      RB_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_STICK_L, LS_CLK_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_STICK_R, RS_CLK_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_PLUS,    PLAY_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_MINUS,   BACK_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_HOME,    SPECIAL_FLAG);
#undef CHECKBTN

        // If the button was just pressed, reset to current time
        if (kpad_data.pro.trigger & WPAD_PRO_BUTTON_HOME)
          home_pressed[controllerNumber] = millis();

        if (btns & WPAD_PRO_BUTTON_HOME && millis() - home_pressed[controllerNumber] > 3000) {
          state = STATE_STOP_STREAM;
          return;
        }

        LiSendMultiControllerEvent(controllerNumber++, gamepad_mask, buttonFlags,
          (kpad_data.pro.hold & WPAD_PRO_TRIGGER_ZL) ? 0xFF : 0,
          (kpad_data.pro.hold & WPAD_PRO_TRIGGER_ZR) ? 0xFF : 0,
          kpad_data.pro.leftStick.x * INT16_MAX, kpad_data.pro.leftStick.y * INT16_MAX,
          kpad_data.pro.rightStick.x * INT16_MAX, kpad_data.pro.rightStick.y * INT16_MAX);
      }
      else if (kpad_data.extensionType == WPAD_EXT_CLASSIC || kpad_data.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
        uint32_t btns = kpad_data.classic.hold;
        short buttonFlags = 0;
#define CHECKBTN(v, f) if (btns & v) buttonFlags |= f;
        if (swap_buttons) {
          CHECKBTN(WPAD_CLASSIC_BUTTON_A,       B_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_B,       A_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_X,       Y_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_Y,       X_FLAG);
        }
        else {
          CHECKBTN(WPAD_CLASSIC_BUTTON_A,       A_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_B,       B_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_X,       X_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_Y,       Y_FLAG);
        }
        CHECKBTN(WPAD_CLASSIC_BUTTON_UP,      UP_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_DOWN,    DOWN_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_LEFT,    LEFT_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_RIGHT,   RIGHT_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_ZL,      LB_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_ZR,      RB_FLAG);
        // don't have stick buttons on a classic controller
        // CHECKBTN(WPAD_CLASSIC_BUTTON_STICK_L, LS_CLK_FLAG);
        // CHECKBTN(WPAD_CLASSIC_BUTTON_STICK_R, RS_CLK_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_PLUS,    PLAY_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_MINUS,   BACK_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_HOME,    SPECIAL_FLAG);
#undef CHECKBTN

        // If the button was just pressed, reset to current time
        if (kpad_data.classic.trigger & WPAD_CLASSIC_BUTTON_HOME)
          home_pressed[controllerNumber] = millis();

        if (btns & WPAD_CLASSIC_BUTTON_HOME && millis() - home_pressed[controllerNumber] > 3000) {
          state = STATE_STOP_STREAM;
          return;
        }

        LiSendMultiControllerEvent(controllerNumber++, gamepad_mask, buttonFlags,
          kpad_data.classic.leftTrigger * 0xFF,
          kpad_data.classic.rightTrigger * 0xFF,
          kpad_data.classic.leftStick.x * INT16_MAX, kpad_data.classic.leftStick.y * INT16_MAX,
          kpad_data.classic.rightStick.x * INT16_MAX, kpad_data.classic.rightStick.y * INT16_MAX);
      }
    }
  }
}

uint32_t n3ds_input_num_controllers(void)
{
  uint32_t numControllers = !disable_gamepad;

  WPADExtensionType type;
  for (int i = 0; i < 4; i++) {
    if (WPADProbe((WPADChan) i, &type) == 0) {
      if (type == WPAD_EXT_PRO_CONTROLLER || type == WPAD_EXT_CLASSIC || type == WPAD_EXT_MPLUS_CLASSIC) {
        numControllers++;
      }
    }
  }

  if (numControllers > 4) {
    numControllers = 4;
  }

  return numControllers;
}

uint32_t n3ds_input_buttons_triggered(void)
{
  uint32_t btns = 0;

  VPADStatus vpad;
  VPADReadError vpad_err;
  VPADRead(VPAD_CHAN_0, &vpad, 1, &vpad_err);
  if (vpad_err == VPAD_READ_SUCCESS) {
    btns |= vpad.trigger;
  }

  KPADStatus kpad_data = {0};
	int32_t kpad_err = -1;
	for (int i = 0; i < 4; i++) {
		KPADReadEx((KPADChan) i, &kpad_data, 1, &kpad_err);
		if (kpad_err == KPAD_ERROR_OK) {
      if (kpad_data.extensionType == WPAD_EXT_PRO_CONTROLLER) {
#define MAPBTNS(b, v) if (kpad_data.pro.trigger & b) btns |= v;
        MAPBTNS(WPAD_PRO_BUTTON_UP,       VPAD_BUTTON_UP);
        MAPBTNS(WPAD_PRO_BUTTON_LEFT,     VPAD_BUTTON_LEFT);
        MAPBTNS(WPAD_PRO_TRIGGER_ZR,      VPAD_BUTTON_ZR);
        MAPBTNS(WPAD_PRO_BUTTON_X,        VPAD_BUTTON_X);
        MAPBTNS(WPAD_PRO_BUTTON_A,        VPAD_BUTTON_A);
        MAPBTNS(WPAD_PRO_BUTTON_Y,        VPAD_BUTTON_Y);
        MAPBTNS(WPAD_PRO_BUTTON_B,        VPAD_BUTTON_B);
        MAPBTNS(WPAD_PRO_TRIGGER_ZL,      VPAD_BUTTON_ZL);
        MAPBTNS(WPAD_PRO_TRIGGER_R,       VPAD_BUTTON_R);
        MAPBTNS(WPAD_PRO_BUTTON_PLUS,     VPAD_BUTTON_PLUS);
        MAPBTNS(WPAD_PRO_BUTTON_HOME,     VPAD_BUTTON_HOME);
        MAPBTNS(WPAD_PRO_BUTTON_MINUS,    VPAD_BUTTON_MINUS);
        MAPBTNS(WPAD_PRO_TRIGGER_L,       VPAD_BUTTON_L);
        MAPBTNS(WPAD_PRO_BUTTON_DOWN,     VPAD_BUTTON_DOWN);
        MAPBTNS(WPAD_PRO_BUTTON_RIGHT,    VPAD_BUTTON_RIGHT);
        MAPBTNS(WPAD_PRO_BUTTON_STICK_R,  VPAD_BUTTON_STICK_R);
        MAPBTNS(WPAD_PRO_BUTTON_STICK_L,  VPAD_BUTTON_STICK_L);
#undef MAPBTNS
      }
      else if (kpad_data.extensionType == WPAD_EXT_CLASSIC || kpad_data.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
#define MAPBTNS(b, v) if (kpad_data.classic.trigger & b) btns |= v;
        MAPBTNS(WPAD_CLASSIC_BUTTON_UP,     VPAD_BUTTON_UP);
        MAPBTNS(WPAD_CLASSIC_BUTTON_LEFT,   VPAD_BUTTON_LEFT);
        MAPBTNS(WPAD_CLASSIC_BUTTON_ZR,     VPAD_BUTTON_ZR);
        MAPBTNS(WPAD_CLASSIC_BUTTON_X,      VPAD_BUTTON_X);
        MAPBTNS(WPAD_CLASSIC_BUTTON_A,      VPAD_BUTTON_A);
        MAPBTNS(WPAD_CLASSIC_BUTTON_Y,      VPAD_BUTTON_Y);
        MAPBTNS(WPAD_CLASSIC_BUTTON_B,      VPAD_BUTTON_B);
        MAPBTNS(WPAD_CLASSIC_BUTTON_ZL,     VPAD_BUTTON_ZL);
        MAPBTNS(WPAD_CLASSIC_BUTTON_R,      VPAD_BUTTON_R);
        MAPBTNS(WPAD_CLASSIC_BUTTON_PLUS,   VPAD_BUTTON_PLUS);
        MAPBTNS(WPAD_CLASSIC_BUTTON_HOME,   VPAD_BUTTON_HOME);
        MAPBTNS(WPAD_CLASSIC_BUTTON_MINUS,  VPAD_BUTTON_MINUS);
        MAPBTNS(WPAD_CLASSIC_BUTTON_L,      VPAD_BUTTON_L);
        MAPBTNS(WPAD_CLASSIC_BUTTON_DOWN,   VPAD_BUTTON_DOWN);
        MAPBTNS(WPAD_CLASSIC_BUTTON_RIGHT,  VPAD_BUTTON_RIGHT);
#undef MAPBTNS
      }
      else {
        // meh we can't really map a wiimote to gamepad
      }
    }
  }

  return btns;
}

static void alarm_callback(OSAlarm* alarm, OSContext* ctx)
{
  n3ds_input_update();
}

static int input_thread_proc()
{
  OSCreateAlarm(&inputAlarm);
  OSSetPeriodicAlarm(&inputAlarm, 0, INPUT_UPDATE_RATE, alarm_callback);

  while (thread_running) {
    OSWaitAlarm(&inputAlarm);
  }
}

static void thread_deallocator(OSThread *thread, void *stack)
{
  free(stack);
}

void start_input_thread(void)
{
  thread_running = 1;
  int stack_size = 4 * 1024 * 1024;
  void* stack_addr = (uint8_t *)memalign(8, stack_size) + stack_size;

  if (!OSCreateThread(&inputThread,
                      input_thread_proc,
                      0, NULL,
                      stack_addr, stack_size,
                      0x10, OS_THREAD_ATTRIB_AFFINITY_ANY))
  {
    return;
  }

  OSSetThreadDeallocator(&inputThread, thread_deallocator);
  OSResumeThread(&inputThread);
}

void stop_input_thread(void)
{
  thread_running = 0;
  OSCancelAlarm(&inputAlarm);
  OSJoinThread(&inputThread, NULL);
}
