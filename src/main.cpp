#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef size_t usize;


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
static KBDLLHOOKSTRUCT key_data = {}; 

LRESULT mouse_hook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
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
#define EVENTS_PER_SECOND 1



int main(void)
{
    HHOOK key_hook_handle = SetWindowsHookExA(WH_KEYBOARD_LL, key_hook, nullptr, 0);
    // HHOOK mouse_hook_handle = SetWindowsHookExA(WH_MOUSE_LL, mouse_hook, nullptr, 0);

    bool running = true;
    Program_State program_state = NORMAL;

    u64 target_time = 1000000 / EVENTS_PER_SECOND;

    u64 curr;
    u64 prev = get_micro_time();
    u64 dt = 0; // dt in microseconds 10^-6 seconds



    MSLLHOOKSTRUCT last_md = mouse_data;
    KBDLLHOOKSTRUCT last_kd = key_data; 

    usize i = 0;

    while (running)
    {
        curr = get_micro_time();
        dt += curr - prev;
        prev = curr;

        // maybe mutex lock
        MSLLHOOKSTRUCT md = mouse_data; 
        KBDLLHOOKSTRUCT kd = key_data; 



        bool is_down = !(kd.flags & 0x80);
        bool changed = kd.flags != last_kd.flags || kd.scanCode != last_kd.scanCode;
        bool was_down = is_down && (!(last_kd.flags & 0x80)) && kd.scanCode == last_kd.scanCode;
        bool pressed = is_down && !was_down;

        bool normal_press = pressed && kd.vkCode == VK_ESCAPE;
        bool command_press = pressed && kd.vkCode == VK_F8;
        bool save_press = pressed && kd.vkCode == 'S';
        bool load_press = pressed && kd.vkCode == 'L';
        bool record_press = pressed && kd.vkCode == 'R';
        bool play_press = pressed && kd.vkCode == 'P';


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
                if (command_press)
                {
                    program_state = COMMAND;
                    printf("command\n");
                }
            } break;
            case COMMAND:
            {
                if (normal_press)
                {
                    program_state = NORMAL;
                    printf("normal\n");
                }
                else if (save_press)
                {
                    program_state = SAVE;
                    printf("save\n");
                }
                else if (load_press)
                {
                    program_state = LOAD;
                    printf("load\n");
                }
                else if (record_press)
                {
                    program_state = RECORDING;
                    memset(temp_input.data, 0, sizeof(temp_input.data));
                    temp_input.count = 0;
                    printf("recording\n");
                }
                else if (play_press)
                {
                    program_state = PLAYING;
                    printf("playing\n");
                    i = 0;
                }
            } break;
            case SAVE:
            {
                if (normal_press)
                {
                    program_state = NORMAL;   
                    printf("normal\n");
                } 
                else if (pressed)
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

            } break;
            case LOAD:
            {
                if (normal_press)
                {
                    program_state = NORMAL;
                    printf("normal\n");
                }
                else if (pressed)
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
            } break;
            case RECORDING:
            {
                if (normal_press)
                {
                    program_state = NORMAL;
                    printf("normal\n");
                    break;
                }

                if (changed && temp_input.count < ARRAY_SIZE(temp_input.data))
                {
                    INPUT key_input;
                    key_input.type = INPUT_KEYBOARD;
                    key_input.ki.wVk = (WORD)kd.vkCode;
                    key_input.ki.wScan = (WORD)kd.scanCode;
                    key_input.ki.dwFlags = kd.flags;
                    key_input.ki.time = kd.time;

                    temp_input.data[temp_input.count++] = key_input; 
                }

            } break;
            case PLAYING:
            {
                if (normal_press)
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
        last_kd = kd;
        MSG m; 
        PeekMessage(&m, nullptr, 0, 0, PM_REMOVE);
    }

    // UnhookWindowsHookEx(mouse_hook_handle);
    UnhookWindowsHookEx(key_hook_handle);
    return 0;
}


