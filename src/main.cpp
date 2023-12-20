#include <windows.h>
#include <stdio.h>
#include <stdint.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;





u64 get_micro_time() 
{
    FILETIME time;
    GetSystemTimePreciseAsFileTime(&time);

    return ((((u64)time.dwHighDateTime) << 32) + (u64)time.dwLowDateTime) / 10;    
}




INPUT inputs[1] = {};
volatile u32 input_count = 0;


#define ARRAY_SIZE(p) (sizeof(p)/sizeof((p)[0]))

volatile bool running = false;


LRESULT mouse_hook(int code, WPARAM wParam, LPARAM lParam)
{
    MSLLHOOKSTRUCT *md = (MSLLHOOKSTRUCT *)lParam;
    if (code != HC_ACTION)
    {
        goto end;
    }

    if (input_count < ARRAY_SIZE(inputs))
    {
        INPUT mouse_input;

        key_input.type = INPUT_MOUSE;

        inputs[input_count++] = mouse_input; 
    }

    



    end:;
    return CallNextHookEx(nullptr, code, wParam, lParam);
}   
LRESULT key_hook(int code, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT *kd = (KBDLLHOOKSTRUCT *)lParam;
    if (code != HC_ACTION)
    {
        goto end;
    }

    if (kd->vkCode == VK_ESCAPE)
    {
        running = false;
        goto end;
    }

    if (input_count < ARRAY_SIZE(inputs))
    {
        INPUT key_input;

        key_input.type = INPUT_KEYBOARD;
        key_input.ki.wVk = (WORD)kd->vkCode;
        key_input.ki.wScan = (WORD)kd->scanCode;
        key_input.ki.dwFlags = kd->flags;
        key_input.ki.time = kd->time;

        inputs[input_count++] = key_input; 
    }


    printf("vkCode %lu\nscanCode %lu\nflags %lu\ntime %lu\ndwExtraInfo %llu\n",
        kd->vkCode, kd->scanCode, kd->flags, kd->time, kd->dwExtraInfo);

    



    end:;
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

#define EVENTS_PER_SECOND 1

int main(void)
{
    HHOOK key_hook_handle = SetWindowsHookExA(WH_KEYBOARD_LL, key_hook, nullptr, 0);
    HHOOK mouse_hook_handle = SetWindowsHookExA(WH_MOUSE_LL, mouse_hook, nullptr, 0);
    // SendInput()
    printf("size %llu\n", ARRAY_SIZE(inputs));
    while (input_count != ARRAY_SIZE(inputs))
    {
        MSG m;
        PeekMessage(&m, nullptr, 0, 0, PM_REMOVE);
        // keyhook adds inputs to inputs array
    }


    running = true;
    u64 target_time = 1000000 / EVENTS_PER_SECOND;

    u64 curr;
    u64 prev = get_micro_time();
    u64 dt; // dt in microseconds 10^-6 seconds


    while (running)
    {
        curr = get_micro_time();
        dt = curr - prev;
        prev = curr;

        SendInput(input_count, inputs, sizeof(INPUT));

        do
        {
            curr = get_micro_time();
            dt = curr - prev;
        } while (dt < target_time);
    }

    UnhookWindowsHookEx(mouse_hook_handle);
    UnhookWindowsHookEx(key_hook_handle);
    return 0;
}


