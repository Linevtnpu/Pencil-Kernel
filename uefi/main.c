// #include <Uefi.h>
// #include <Library/UefiBootServicesTableLib.h>
// #include <Protocol/GraphicsOutput.h>
// #include <Protocol/SimpleFileSystem.h>
// #include <Guid/FileInfo.h>

#include <Efi.h>

#include "common.h"

#include "file.h"
#include "lib.h"
#include "video.h"

EFI_BOOT_SERVICES*               gBS;
EFI_GRAPHICS_OUTPUT_PROTOCOL*    Gop;
EFI_SYSTEM_TABLE*                gST;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Sfsp;
EFI_HANDLE                       gImageHandle;

EFI_GUID gEfiGraphicsOutputProtocolGuid   = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiFileInfoGuid                 = EFI_FILE_INFO_ID;

typedef struct
{
    int hr;
    int vr;
    CHAR16 KernelName[16];
} BootConfig;

void ReadConfig(EFI_PHYSICAL_ADDRESS FileBase,BootConfig* Config);
void PrepareBootInfo(struct BootInfo* Binfo,EFI_PHYSICAL_ADDRESS KernelBase,EFI_PHYSICAL_ADDRESS CharacterBase);
UINTN abs(INTN a);

EFI_STATUS
EFIAPI
UefiMain
(
    IN EFI_HANDLE ImageHandle __attribute__((unused)),
    IN EFI_SYSTEM_TABLE* SystemTable
)
{
    gImageHandle = ImageHandle;
    gBS = SystemTable->BootServices;
    gST = SystemTable;
    /* 禁用计时器 */
    SystemTable->BootServices->SetWatchdogTimer(0,0,0,NULL);
    
    gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID**)&Gop);
    gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid,NULL,(VOID**)&Sfsp);

    /* 设置显示模式 */
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut); /* 清屏 */
    SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Pencil-Boot\n\r");

    EFI_PHYSICAL_ADDRESS FileBase;
    while(EFI_ERROR(ReadFile(L"\\BootConfig.txt",&FileBase,AllocateAnyPages)))
    {
        gST->ConOut->OutputString(SystemTable->ConOut,L"Read 'BootConfig.txt' failed. Press Any key to try again.");
        get_char();
    }
    BootConfig Config;
    ReadConfig(FileBase,&Config);

    CHAR16 str[30];

    utoa(Config.hr,str,10);
    SystemTable->ConOut->OutputString(SystemTable->ConOut,L"x: ");
    SystemTable->ConOut->OutputString(SystemTable->ConOut,str);
    SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\n\r");

    utoa(Config.vr,str,10);
    SystemTable->ConOut->OutputString(SystemTable->ConOut,L"y: ");
    SystemTable->ConOut->OutputString(SystemTable->ConOut,str);
    SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\n\r");

    SystemTable->ConOut->OutputString(SystemTable->ConOut,L"kernel Name: ");
    SystemTable->ConOut->OutputString(SystemTable->ConOut,Config.KernelName);

    UINTN SizeOfInfo = 0;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN i;
    int Mode = 0;
    UINTN sub = 0xffffffff;
    for(i = 0;i < Gop->Mode->MaxMode;i++)
    {
        Gop->QueryMode(Gop,i,&SizeOfInfo,&Info);
        if(abs(Config.hr - Info->HorizontalResolution) + abs(Config.vr -Info->VerticalResolution) < sub)
        {
            sub = abs(Config.hr - Info->HorizontalResolution) + abs(Config.vr - Info->VerticalResolution);
            Mode = i;
        }
    }
    Gop->SetMode(Gop,Mode);
    while(1)
    {
        /* 提示符 */
        SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Pencil Boot >");
        /* 等待,直到发生输入 */
        str[0] = L'\0';
        get_line(str,30);
        if(strcmp(str,L"") == 0)
        {
            continue;
        }
        else if(strcmp(str,L"shutdown") == 0)
        {
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Press any key to shutdown...\n\r");
            get_char();
            SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,EFI_SUCCESS,0,NULL);
            return 0;
        }
        else if(strcmp(str,L"cls") == 0)
        {
            SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
        }
        else if(strcmp(str,L"boot") == 0)
        {
            /* 读取内核文件,默认加载到0x100000,若地址被占用,则另分配地址
            * 实际加载到的地址在BootInfo->KernelBaseAddress中 */
            EFI_PHYSICAL_ADDRESS KernelFileBase = 0x100000;
            if(EFI_ERROR(ReadFile(Config.KernelName,&KernelFileBase,AllocateAddress)))
            {
                if(EFI_ERROR(ReadFile(Config.KernelName,&KernelFileBase,AllocateAnyPages)))
                {
                    continue;
                }
            }
            EFI_PHYSICAL_ADDRESS CharacterBase;
            if(EFI_ERROR(ReadFile(L"\\utf8.sys",&CharacterBase,AllocateAnyPages)))
            {

            }
            utoa(KernelFileBase,str,16);
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"KernelFileBase: ");
            SystemTable->ConOut->OutputString(SystemTable->ConOut,str);
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\n\r");

            utoa(CharacterBase,str,16);
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Character Base: ");
            SystemTable->ConOut->OutputString(SystemTable->ConOut,str);
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\n\r");

            EFI_STATUS (*Kernel)(struct BootInfo*) = (EFI_STATUS(*)(struct BootInfo*))KernelFileBase;
            struct BootInfo BootInfo;
            PrepareBootInfo(&BootInfo,KernelFileBase,CharacterBase);
            UINTN PassBack = Kernel(&BootInfo);
            utoa(PassBack,str,16);
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Kernel Return: ");
            SystemTable->ConOut->OutputString(SystemTable->ConOut,str);
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\n\r");
        }
        else
        {
            SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Not a command.\n\r");
        }
    }
    return 0;
}

UINTN abs(INTN a)
{
    return (a < 0 ) ? -a : a;
}

void ReadConfig(EFI_PHYSICAL_ADDRESS FileBase,BootConfig* Config)
{
    gST->ConOut->OutputString(gST->ConOut,L"Reading Config...\n\r");
    CHAR16* s = (CHAR16*)FileBase;
    while(*s)
    {
        if(*s == L'.')
        {
            s++;
            if(*s == L'x' || *s == L'X')
            {
                s += 2;
                Config->hr = 0;
                while(*s <= L'9' && *s >= L'0')
                {
                    Config->hr = Config->hr * 10 + *(s++) - L'0'; 
                }
            }
            else if(*s == L'y' || *s == L'Y')
            {
                s += 2;
                Config->vr = 0;
                while(*s <= L'9' && *s >= L'0')
                {
                    Config->vr = Config->vr * 10 + *(s++) - L'0'; 
                }
            }
            else if(*s == L'k' || *s == L'K')
            {
                s += 3;
                int i = 0;
                while((Config->KernelName[i++] = *s++) != L'\"');
                Config->KernelName[i - 1] = L'\0';
            }
        }
        s++;
    }
}

void PrepareBootInfo(struct BootInfo* Binfo,EFI_PHYSICAL_ADDRESS KernelBase,EFI_PHYSICAL_ADDRESS CharacterBase)
{
    Binfo->KernelBaseAddress                   = KernelBase;
    Binfo->CharacterBase                       = CharacterBase;
    Binfo->GraphicsInfo.FrameBufferBase        = Gop->Mode->FrameBufferBase;
    Binfo->GraphicsInfo.HorizontalResolution   = Gop->Mode->Info->HorizontalResolution;
    Binfo->GraphicsInfo.VerticalResolution     = Gop->Mode->Info->VerticalResolution;
    Binfo->GraphicsInfo.PixelBitMask.RedMask   = Gop->Mode->Info->PixelInformation.RedMask;
    Binfo->GraphicsInfo.PixelBitMask.GreenMask = Gop->Mode->Info->PixelInformation.GreenMask;
    Binfo->GraphicsInfo.PixelBitMask.BlueMask  = Gop->Mode->Info->PixelInformation.BlueMask;
}