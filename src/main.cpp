#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef size_t usize;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;


u64 get_micro_time() 
{
    FILETIME time;
    GetSystemTimePreciseAsFileTime(&time);

    return ((((u64)time.dwHighDateTime) << 32) + (u64)time.dwLowDateTime) / 10;    
}

#define MAX_SLOTS 10
#define MAX_INPUT_LENGTH 16


struct Input_Data
{
    INPUT data[MAX_INPUT_LENGTH];
    u32 count;
};

enum Program_State
{
    NORMAL,
    COMMAND,
    SAVE,
    LOAD,
    RECORDING,
    PLAYING,
};


static MSLLHOOKSTRUCT mouse_data = {};
static WPARAM mouse_wparam = {};

static KBDLLHOOKSTRUCT key_data = {}; 



LRESULT mouse_hook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        mouse_wparam = wParam;
        mouse_data = *(MSLLHOOKSTRUCT *)lParam;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT key_hook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        key_data = *(KBDLLHOOKSTRUCT *)lParam;
        // printf("key press\n");
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static Input_Data temp_input = {};
static Input_Data inputs[MAX_SLOTS] = {};


#define ARRAY_SIZE(p) (sizeof(p)/sizeof((p)[0]))
#define DEFAULT_EVENTS_PER_SECOND 1

u64 clamp(u64 min, u64 a, u64 max)
{
    if (a > max) return max;
    if (a < min) return min;
    return a;
}

int main(void)
{
    // sets dpi so screen_width/height is scaled
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE))
    {
        printf("failed to set dpi awareness context\n");
        return -1;
    }

    HHOOK key_hook_handle = SetWindowsHookExA(WH_KEYBOARD_LL, key_hook, nullptr, 0);
    HHOOK mouse_hook_handle = SetWindowsHookExA(WH_MOUSE_LL, mouse_hook, nullptr, 0);
    // HHOOK mouse_hook_handle;
    u16 screen_width = (u16) GetSystemMetrics(SM_CXSCREEN);
    u16 screen_height = (u16) GetSystemMetrics(SM_CYSCREEN);



    bool running = true;
    Program_State program_state = NORMAL;

    u64 target_time = 1000000 / DEFAULT_EVENTS_PER_SECOND;

    u64 curr;
    u64 prev = get_micro_time();
    u64 dt = 0; // dt in microseconds 10^-6 seconds



    MSLLHOOKSTRUCT last_md = mouse_data;
    WPARAM last_mw = mouse_wparam;
    KBDLLHOOKSTRUCT last_kd = key_data; 

    usize i = 0;



    while (running)
    {
        curr = get_micro_time();
        dt += curr - prev;
        prev = curr;

        // maybe mutex lock
        MSLLHOOKSTRUCT md = mouse_data; 
        WPARAM mw = mouse_wparam;
        KBDLLHOOKSTRUCT kd = key_data; 


        bool mouse_changed = mw != last_mw;

        bool is_down = !(kd.flags & 0x80);
        bool changed = kd.flags != last_kd.flags || kd.scanCode != last_kd.scanCode;
        bool was_down = is_down && (!(last_kd.flags & 0x80)) && kd.scanCode == last_kd.scanCode;
        bool pressed = is_down && !was_down;

        bool normal = kd.vkCode == VK_ESCAPE;
        bool speed_up = kd.vkCode == VK_OEM_PLUS;
        bool speed_down = kd.vkCode == VK_OEM_MINUS;
        bool command = kd.vkCode == VK_F8;
        bool save = kd.vkCode == 'S';
        bool load = kd.vkCode == 'L';
        bool record = kd.vkCode == 'R';
        bool play = kd.vkCode == 'P';



        // if (changed)
        // {
        //     printf("vkCode %lu scanCode %lu flags %lu time %lu dwExtraInfo %llu\n",
        //         kd.vkCode, kd.scanCode, kd.flags, kd.time, kd.dwExtraInfo);
        // }

        if (is_down && kd.vkCode == VK_F3)
        {
            running = false;
            program_state = NORMAL;
        }

        switch (program_state)
        {
            case NORMAL:
            {
                if (pressed)
                {
                    if (command)
                    {
                        program_state = COMMAND;
                        printf("command\n");
                    }

                }
            } break;
            case COMMAND:
            {
                if (pressed)
                {
                    if (normal)
                    {
                        program_state = NORMAL;
                        printf("normal\n");
                    }
                    else if (speed_up)
                    {
                        u64 events_per_second = 1000000 / target_time;
                        events_per_second = clamp(1, events_per_second + 1, 100);
                        target_time = 1000000 / events_per_second;
                        printf("speed %llu\n", events_per_second);
                    }
                    else if (speed_down)
                    {
                        u64 events_per_second = 1000000 / target_time;
                        events_per_second = clamp(1, events_per_second - 1, 100);
                        target_time = 1000000 / events_per_second;
                        printf("speed %llu\n", events_per_second);
                    }
                    else if (save)
                    {
                        program_state = SAVE;
                        printf("save\n");
                    }
                    else if (load)
                    {
                        program_state = LOAD;
                        printf("load\n");
                    }
                    else if (record)
                    {
                        program_state = RECORDING;
                        memset(temp_input.data, 0, sizeof(temp_input.data));
                        temp_input.count = 0;
                        printf("recording\n");
                    }
                    else if (play)
                    {
                        program_state = PLAYING;
                        printf("playing\n");
                        i = 0;
                    }

                }
            } break;
            case SAVE:
            {
                if (pressed)
                {
                    if (normal)
                    {
                        program_state = NORMAL;   
                        printf("normal\n");
                    } 
                    else
                    {
                        if (kd.vkCode >= '0' && kd.vkCode <= '9')
                        {
                            u32 slot = kd.vkCode - '0'; 
                            inputs[slot] = temp_input;
                            printf("saved to slot %u\n", slot);
                        }
                        else
                        {
                            printf("invalid slot use 0-9\n");
                        }
                    }
                }

            } break;
            case LOAD:
            {
                if (pressed)
                {
                    if (normal)
                    {
                        program_state = NORMAL;
                        printf("normal\n");
                    }
                    else
                    {
                        if (kd.vkCode >= '0' && kd.vkCode <= '9')
                        {
                            u32 slot = kd.vkCode - '0'; 
                            temp_input = inputs[slot];
                            printf("loaded from slot %u\n", slot);
                        }
                        else
                        {
                            printf("invalid slot use 0-9\n");
                        }
                    }
                }
            } break;
            case RECORDING:
            {
                if (pressed && normal)
                {
                    program_state = NORMAL;
                    printf("normal\n");
                    break;
                }

                if (temp_input.count < ARRAY_SIZE(temp_input.data))
                {
                    if (pressed)
                    {
                        INPUT key_input;
                        key_input.type = INPUT_KEYBOARD;
                        key_input.ki.wVk = (WORD)kd.vkCode;
                        key_input.ki.wScan = (WORD)kd.scanCode;
                        key_input.ki.dwFlags = kd.flags;
                        key_input.ki.time = kd.time;

                        temp_input.data[temp_input.count++] = key_input; 
                    }
                    if (mouse_changed)
                    {
                        INPUT mouse_input = {};
                        mouse_input.type = INPUT_MOUSE;


                        mouse_input.mi.dx = md.pt.x * 65535 / (s32)screen_width;
                        mouse_input.mi.dy = md.pt.y * 65535 / (s32)screen_height;
                        if (mw != WM_MOUSEMOVE)
                        {
                            printf("x %ld, y %ld, ", md.pt.x, md.pt.y);
                            printf("mx %ld, my %ld\n", mouse_input.mi.dx, mouse_input.mi.dy);
                        }
                        // mouse_input.mi.mouseData;
                        mouse_input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE;



                        switch (mw)
                        {
                            case WM_LBUTTONDOWN: mouse_input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN; break;
                            case WM_LBUTTONUP: mouse_input.mi.dwFlags |= MOUSEEVENTF_LEFTUP; break;

                            case WM_RBUTTONDOWN: mouse_input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN; break;
                            case WM_RBUTTONUP: mouse_input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP; break;

                            case WM_MBUTTONDOWN: mouse_input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN; break;
                            case WM_MBUTTONUP: mouse_input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP; break;


                            case WM_XBUTTONDOWN:
                            {
                                mouse_input.mi.dwFlags |= MOUSEEVENTF_XDOWN;
                                u16 xbutton = (u16) (md.mouseData & 0xFFFF0000); 
                                mouse_input.mi.mouseData = xbutton;
                            } break;

                            case WM_XBUTTONUP:
                            {
                                mouse_input.mi.dwFlags |= MOUSEEVENTF_XUP;
                                u16 xbutton = (u16) (md.mouseData & 0xFFFF0000); 
                                mouse_input.mi.mouseData = xbutton;
                            } break;

                            case WM_MOUSEWHEEL:
                            {
                                mouse_input.mi.dwFlags |= MOUSEEVENTF_WHEEL;
                                mouse_input.mi.mouseData = (u16) (md.mouseData & 0xFFFF0000);
                            } break;

                            case WM_MOUSEMOVE:
                            {
                                // do nothing
                            } break;

                            default:
                                printf("%llu\n", mw); 
                                assert(false);
                        }

                        // mouse_input.mi.time;
                        // mouse_input.mi.dwExtraInfo;
                        if (mw != WM_MOUSEMOVE)
                        {
                            temp_input.data[temp_input.count++] = mouse_input;
                        }
                    }
                }


            } break;
            case PLAYING:
            {
                if (pressed && normal)
                {
                    program_state = NORMAL;
                    printf("normal\n");
                    break;
                }

                if (dt >= target_time)
                {
                    dt = 0;
                    if (temp_input.count > 0)
                    {
                        SendInput(1, temp_input.data + i, sizeof(INPUT));
                        i += 1;
                        i %= temp_input.count;
                    }
                }
            } break;
        }

        last_md = md;
        last_mw = mw;
        last_kd = kd;
        MSG m; 
        PeekMessage(&m, nullptr, 0, 0, PM_REMOVE);
    }

    UnhookWindowsHookEx(mouse_hook_handle);
    UnhookWindowsHookEx(key_hook_handle);
    return 0;
}


