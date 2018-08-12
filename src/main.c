/**
 * sys-stfo - Switch fast power off
 * 
 * Copyright (C) 2018 Steven "Drakia" Scott
 * Copyright (C) 2018 jakibaki
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <switch.h>
#include "bpc.h"

#define TITLE_ID 0x420000000000001A
#define HEAP_SIZE 0x000540000

// We aren't an applet
u32 __nx_applet_type = AppletType_None;

// Setup a fake heap
char fake_heap[HEAP_SIZE];

// Override libnx's internal init to do a minimal init
void __libnx_initheap(void)
{
    extern char *fake_heap_start;
    extern char *fake_heap_end;

    // setup newlib fake heap
    fake_heap_start = fake_heap;
    fake_heap_end = fake_heap + HEAP_SIZE;
}

void fatalLater(Result err)
{
    Handle srv;

    while (R_FAILED(smGetServiceOriginal(&srv, smEncodeName("fatal:u"))))
    {
        // wait one sec and retry
        svcSleepThread(1000000000L);
    }

    // fatal is here time, fatal like a boss
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);
    struct
    {
        u64 magic;
        u64 cmd_id;
        u64 result;
        u64 unknown;
    } * raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->result = err;
    raw->unknown = 0;

    ipcDispatch(srv);
    svcCloseHandle(srv);
}

void registerFspLr() {
    if (kernelAbove400())
        return;

    Result rc = fsprInitialize();
    if (R_FAILED(rc))
        fatalLater(rc);

    u64 pid;
    svcGetProcessId(&pid, CUR_PROCESS_HANDLE);

    rc = fsprRegisterProgram(pid, TITLE_ID, FsStorageId_NandSystem, NULL, 0, NULL, 0);
    if (R_FAILED(rc))
        fatalLater(rc);
    fsprExit();
}

void __appInit(void)
{
    Result rc;
    svcSleepThread(10000000000L);
    rc = smInitialize();
    if (R_FAILED(rc))
        fatalLater(rc);
    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalLater(rc);
    rc = bpcInitialize();
    if (R_FAILED(rc))
        fatalLater(rc);
    rc = timeInitialize();
    if (R_FAILED(rc))
        fatalLater(rc);
}

void __appExit(void)
{
    timeExit();
    bpcExit();
    hidExit();
    smExit();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    while (appletMainLoop())
    {
        // Check if the user is pressing Plus + Minus
        hidScanInput();
        u64 bPressed = hidKeysHeld(CONTROLLER_P1_AUTO);
        if (bPressed & KEY_PLUS && bPressed & KEY_MINUS)
        {
            // Try to unmount the SD card, so we hopefully don't corrupt anything
            fsdevUnmountAll();

            // If R is also held down, reboot the system
            if (bPressed & KEY_R)
            {
                bpcRebootSystem();
            }
            // If L is also held down, shut down the system
            else if (bPressed & KEY_L)
            {
                bpcShutdownSystem();
            }
        }
        
        // Sleep so we don't take up too much CPU
        svcSleepThread(100000000L);
    }
    return 0;
}