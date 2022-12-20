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
#include "memory.h"

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
    int kernel_flage;
    CHAR16   KernelName[32];
    int typeface_flage;
    CHAR16 TypefaceName[32];
} BootConfig;

void ReadConfig(EFI_PHYSICAL_ADDRESS FileBase,BootConfig* Config);
void PrepareBootInfo(struct BootInfo* Binfo,struct MemoryMap* memmap,EFI_PHYSICAL_ADDRESS KernelBase,EFI_PHYSICAL_ADDRESS TypefaceBase);
void gotoKernel(BootConfig* Config);

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

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut); /* 清屏 */
    CHAR16* logo = 
    L"     _______   ______   __   __   ______   ______   __       \n\r"
     "    / ___  /| / ____/| /  | / /| / ____/| /_  __/| / /|      \n\r"
     "   / /__/ / // /____|// | |/ / // /|___|/ |/ /|_|// / /      \n\r"
     "  / _____/ // ____/| / /| | / // / /      / / /  / / /       \n\r"
     " / /|____|// /____|// / |  / // /_/__  __/ /_/  / / /__      \n\r"
     "/_/ /     /______/|/_/ /|_/ //______/|/______/|/______/|     \n\r"
     "|_|/      |______|/|_|/ |_|/ |______|/|______|/|______|/     \n\r";
    SystemTable->ConOut->OutputString(SystemTable->ConOut,logo);
    /* 读取启动配置 */
    
    BootConfig Config =
    {
        .hr             = 0,
        .vr             = 0,
        .kernel_flage   = 0,
        .KernelName     = L"\0",
        .typeface_flage = 0,
        .TypefaceName   = L"\0"
    };
    EFI_PHYSICAL_ADDRESS FileBase;
    if(EFI_ERROR(ReadFile(L"\\BootConfig.txt",&FileBase,AllocateAnyPages)))
    {
        gST->ConOut->OutputString(SystemTable->ConOut,L"'BootConfig.txt' not found. Press Any key to continue.\n\r");
        get_char();
    }
    else
    {
        ReadConfig(FileBase,&Config);
    }
    gotoKernel(&Config);
    // CHAR16 str[30];
    // while(1)
    // {
    //     /* 提示符 */
    //     SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Pencil Boot >");
    //     /* 等待,直到发生输入 */
    //     str[0] = L'\0';
    //     get_line(str,30);
    //     if(strcmp(str,L"") == 0)
    //     {
    //         continue;
    //     }
    //     else if(strcmp(str,L"shutdown") == 0)
    //     {
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Press any key to shutdown...\n\r");
    //         get_char();
    //         SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,EFI_SUCCESS,0,NULL);
    //         return 0;
    //     }
    //     else if(strcmp(str,L"cls") == 0)
    //     {
    //         SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    //     }
    //     else if(strcmp(str,L"boot") == 0)
    //     {
    //         gotoKernel(Config);
    //         utoa(PassBack,str,16);
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Kernel Return: ");
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,str);
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,L"\n\r");
    //     }
    //     else
    //     {
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,L"Not a command.\n\r");
    //     }
    // }
    return 0;
}

CHAR16* skip_space(CHAR16* s)
{
    while(*s == L' ' || *s == L'\n' || *s == L'\r')
    {
        s++;
    }
    return s;
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
            else if(strncmp(s,L"kernel",6) == 0)
            {
                s += 6;
                s = skip_space(s);
                if(*s == L'=')
                {
                    s++;
                }
                s = skip_space(s);
                if(*s != L'\"')
                {
                    continue;
                }
                s++;
                int i = 0;
                while((Config->KernelName[i++] = *s++) != L'\"');
                Config->KernelName[i - 1] = L'\0';
                Config->kernel_flage = 1;
            }
            else if(strncmp(s,L"typeface",8) == 0)
            {
                s += 8;
                s = skip_space(s);
                if(*s == L'=')
                {
                    s++;
                }
                s = skip_space(s);
                if(*s != L'\"')
                {
                    continue;
                }
                s++;
                int i = 0;
                while((Config->TypefaceName[i++] = *s++) != L'\"');
                Config->TypefaceName[i - 1] = L'\0';
                Config->typeface_flage = 1;
            }
        }
        s++;
    }
}

void PrepareBootInfo(struct BootInfo* Binfo,struct MemoryMap* memmap,EFI_PHYSICAL_ADDRESS KernelBase,EFI_PHYSICAL_ADDRESS TypefaceBase)
{
    Binfo->KernelBaseAddress                   = KernelBase;
    Binfo->TypefaceBase                       = TypefaceBase;
    Binfo->GraphicsInfo.FrameBufferBase        = Gop->Mode->FrameBufferBase;
    Binfo->GraphicsInfo.HorizontalResolution   = Gop->Mode->Info->HorizontalResolution;
    Binfo->GraphicsInfo.VerticalResolution     = Gop->Mode->Info->VerticalResolution;
    Binfo->GraphicsInfo.PixelBitMask.RedMask   = Gop->Mode->Info->PixelInformation.RedMask;
    Binfo->GraphicsInfo.PixelBitMask.GreenMask = Gop->Mode->Info->PixelInformation.GreenMask;
    Binfo->GraphicsInfo.PixelBitMask.BlueMask  = Gop->Mode->Info->PixelInformation.BlueMask;
    Binfo->MemoryMap = *memmap;
}

void gotoKernel(BootConfig* Config)
{
    /* 读取内核文件,默认加载到0x100000 */
    EFI_PHYSICAL_ADDRESS KernelFileBase = 0x100000;
    if(!Config->kernel_flage)
    {
        gST->ConOut->OutputString(gST->ConOut,L"Press kernel file name:");
        get_line(Config->KernelName,32);
    }
    if(EFI_ERROR(ReadFile(Config->KernelName,&KernelFileBase,AllocateAddress)))
    {
        gST->ConOut->OutputString(gST->ConOut,L"Unable to load kernel (maybe address 0x100000 unavailable) \n\r");
        gST->ConOut->OutputString(gST->ConOut,L"Please restart your computer\n\r");
        while(1);
    }
    EFI_PHYSICAL_ADDRESS TypefaceBase = NULL;
    if(Config->typeface_flage == 1)
    {
        if(EFI_ERROR(ReadFile(Config->TypefaceName,&TypefaceBase,AllocateAnyPages)))
        {
            gST->ConOut->OutputString(gST->ConOut,L"ERROR:typeface file needed but not found. Press any key to continue. \n\r");
            get_char();
            TypefaceBase = NULL;
        }
    }
    /* 设置显示模式 */
    SetVideoMode(Config->hr,Config->vr);
    struct MemoryMap Memmap = 
    {
        .MapSize = 4096 * 4,
        .Buffer = NULL,
        .MapKey = 0,
        .DescriptorSize = 0,
        .DescriptorVersion = 0,
    };
    GetMemoryMap(&Memmap);
    EFI_STATUS (*Kernel)(struct BootInfo*) = (EFI_STATUS(*)(struct BootInfo*))KernelFileBase;
    struct BootInfo BootInfo;
    PrepareBootInfo(&BootInfo,&Memmap,KernelFileBase,TypefaceBase);
    // 退出启动时服务,进入内核
    gBS->ExitBootServices(gImageHandle,Memmap.MapKey);
    Kernel(&BootInfo);
}