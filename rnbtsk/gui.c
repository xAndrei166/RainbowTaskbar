#include <windows.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <windowsx.h>
#include <shellapi.h>
#include <strsafe.h>
#include <tchar.h>
#include <shlwapi.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "shlwapi.lib")

#include "accent.h"
#include "config.h"
#include "filediag.h"
#include "gui.h"
#include "map.h"
#include "switchs.h"
#include "resource.h"

#define WM_NEWTEXT 0xAD2017
#define WC_TEXT L"TEXT"
#define WM_USER_SHELLICON WM_APP+1

typedef struct {
    int x;
    int y;
    int len;
    BOOL enabled;
    LPWSTR text;
} CONTROL_TEXT;

HICON hMainIcon;
HFONT guiFont;
MapDef(controls, HWND);
MapDef(texts, CONTROL_TEXT)
BOOL XY;



int index = 0;
int tindex = 0;
int r, g, b = 0;
int which = 0;
HBRUSH transparent = 0;
COLORREF bgc = RGB(0x22, 0x22, 0x22);

LRESULT CALLBACK GUIProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Config_ParseLine(char* ln);
void ConfView_Init(HWND confview);
void Cycle();
void InitWnd();

HWND Control(LPCSTR name, LPTSTR class, LPTSTR text, DWORD style, int x, int y, int w, int h) {
    if (!wcscmp(class, WC_TEXT)) { // fuck win32 text 
        CONTROL_TEXT txt = { 0 };
        txt.len = wcslen(text);
        txt.text = text;
        txt.x = x;
        txt.y = y;
        txt.enabled = TRUE;

        MapAdd(texts, name, txt);

        return tindex++;
        SendMessage(mainn, WM_NEWTEXT, NULL, NULL);
    }
    HWND a = CreateWindowExW(!strcmp(class, WC_EDIT) || !strcmp(class, WC_TREEVIEW) ? WS_EX_CLIENTEDGE : 0L,
        class,
        text,
        style | WS_VISIBLE | WS_CHILD, 
        x, 
        y,
        w,
        h,
        mainn,
        index++, 
        GetModuleHandleA(NULL),
        NULL);
    SendMessage(a, WM_SETFONT, (LPARAM)guiFont, TRUE);
    //SetBkMode(GetDC(a), TRANSPARENT);
    MapAdd(controls, name, a);
    return a;
}

void EnableControl(HWND ctrl, BOOL enable) {
    EnableWindow(ctrl, enable);
    ShowWindow(ctrl, enable);
}

void TextModify(int index, LPTSTR text) {
    MapArray(texts)[index].text = text;
    MapArray(texts)[index].len = wcslen(text);

    RedrawWindow(mainn, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}
void EnableText(int index, BOOL enable) {
    MapArray(texts)[index].enabled = enable;
    RedrawWindow(mainn, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}


void Class(wchar_t* name, HANDLE hInstance, WNDPROC proc) {
    WNDCLASS wc = { 0 };

    wc.lpfnWndProc = proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = name;


    RegisterClass(&wc);
    
}
rtcfg* cfg;
// pasted code from https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
char* ExecCmd(const wchar_t* cmd, char* buf)
{
    char* strResult = NULL;
    HANDLE hPipeRead, hPipeWrite;

    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };
    saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child process.
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe to get results from child's stdout.
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
        return strResult;

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;
    si.wShowWindow = SW_HIDE; // Prevents cmd window from flashing.
                              // Requires STARTF_USESHOWWINDOW in dwFlags.

    PROCESS_INFORMATION pi = { 0 };

    BOOL fSuccess = CreateProcessW(NULL, (LPWSTR)cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    if (!fSuccess)
    {
        CloseHandle(hPipeWrite);
        CloseHandle(hPipeRead);
        return strResult;
    }

    BOOL bProcessEnded = FALSE;
    for (; !bProcessEnded;)
    {
        // Give some timeslice (50 ms), so we won't waste 100% CPU.
        bProcessEnded = WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0;

        // Even if process exited - we continue reading, if
        // there is some data available over pipe.
        for (;;)
        {
            DWORD dwRead = 0;
            DWORD dwAvail = 0;

            if (!PeekNamedPipe(hPipeRead, NULL, 0, NULL, &dwAvail, NULL))
                break;

            if (!dwAvail) // No data available, return
                break;

            if (!ReadFile(hPipeRead, buf, min(1024 - 1, dwAvail), &dwRead, NULL) || !dwRead)
                // Error, the child process might ended
                break;

            buf[dwRead] = 0;
            strResult = buf;
        }
    }

    CloseHandle(hPipeWrite);
    CloseHandle(hPipeRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return strResult;
}

DWORD CheckUpdate() {
    char data[1024];
    ExecCmd(L"cmd /c curl --help 1> nul 2> nul && echo %ERRORLEVEL%", data);
    if (!memcmp(data, "9009", 4)) {
        return;
    }
    ExecCmd(L"curl -m 10 -s https://ad2017.dev/rnbver.txt", data);
    HKEY hkey = NULL;
    RegOpenKey(HKEY_CURRENT_USER, _T("Software\\RainbowTaskbarNoUpdate"), &hkey);
    if (hkey != NULL) {
        return 1;
    }
    if (strcmp(data, RNBVER)) {
        // UPDATE!
        UINT ret = MessageBox(0, L"An update for RainbowTaskbar has been released. Do you want to update? (Click CANCEL if you don't want to be asked again)", L"Update", MB_YESNOCANCEL | MB_ICONINFORMATION);
        if (ret == IDYES) {
            wchar_t szFileName[MAX_PATH];
            GetModuleFileNameW(NULL, szFileName, MAX_PATH);
            wchar_t* filename = PathFindFileNameW(szFileName);
            wchar_t cmd[MAX_PATH * 3];
            memset(cmd, 0, sizeof(wchar_t) * MAX_PATH * 3);
            wcscat(cmd, L"cmd /c taskkill /f /im ");
            wcscat(cmd, filename);
            wcscat(cmd, L" && del /q /f ");
            wcscat(cmd, szFileName);
            wcscat(cmd, L" && curl -L ");
            if (RNBARCH == 64) {
                wcscat(cmd, L"https://github.com/ad2017gd/RainbowTaskbar/releases/latest/download/rnbtsk-x64.exe");
            }
            else {
                wcscat(cmd, L"https://github.com/ad2017gd/RainbowTaskbar/releases/latest/download/rnbtsk.exe");
            }
            wcscat(cmd, L" -o ");
            wcscat(cmd, szFileName);
            wcscat(cmd, L" && ");
            wcscat(cmd, szFileName);

            char nothing[1024];
            ExecCmd(cmd, nothing);
        }
        else if (ret == IDCANCEL) {
            RegCreateKey(HKEY_CURRENT_USER, _T("Software\\RainbowTaskbarNoUpdate"), &hkey);
        }
    }
    return 0;
}

void RnbTskGUI(HINSTANCE hInstance)
{
    MapInit(controls);
    MapInit(texts);
    CreateThread(0,0,(LPTHREAD_START_ROUTINE)CheckUpdate,0,0,0);

    transparent = CreateSolidBrush(bgc);

    Class(L"RnbTskGui", hInstance, GUIProc);

    mainn = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_DLGMODALFRAME,
        L"RnbTskGui",
        L"",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 620,

        NULL,
        NULL,
        hInstance,
        NULL
    );

    

    NONCLIENTMETRICS metrics;
    metrics.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS),
        &metrics, 0);
    guiFont = CreateFont(20, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft Sans Serif");
    SendMessage(mainn, WM_SETFONT, (LPARAM)guiFont, TRUE);

    InitWnd();


    //SetWindowABlur(mainn, 3, ARGB(64, 128, 0, 128));
    ShowWindow(mainn, FALSE);



    HICON hMainIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, 0);

    SendMessage(mainn, WM_SETICON, ICON_BIG, (LPARAM)hMainIcon);
    SendMessage(mainn, WM_SETICON, ICON_SMALL, (LPARAM)hMainIcon);

    NOTIFYICONDATA nidApp = { 0 };

    nidApp.cbSize = sizeof(NOTIFYICONDATA);
    nidApp.hWnd = (HWND)mainn;
    nidApp.uID = 0xAD2017;
    nidApp.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nidApp.hIcon = hMainIcon;
    nidApp.uCallbackMessage = WM_USER_SHELLICON;
    WCHAR wide[sizeof(RNBVER) * 2];
    mbstowcs(wide, RNBVER, sizeof(RNBVER) * 2);
    WCHAR real[32];
    wsprintfW(real, L"%s %s", L"RainbowTaskbar", wide);
    StringCchCopy(nidApp.szTip, ARRAYSIZE(nidApp.szTip), real);
    Shell_NotifyIcon(NIM_ADD, &nidApp);



    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    MapDelete(controls);
    MapDelete(texts);
    free(cfg);
    DeleteObject(guiFont);

    return 0;
}


DWORD rgbCurr = 0;
DWORD rgbCurr2 = 0;

COLORREF PickColor(int which) {
    CHOOSECOLOR cc = { 0 };
    COLORREF acrCustClr[16]; 
    HBRUSH hbrush; 

    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = mainn;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = which ? rgbCurr2 : rgbCurr;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc) == TRUE)
    {
        return cc.rgbResult;
    }
    return 0;
}

void Control_HideAll() {
    HWND ctrl;
    CONTROL_TEXT unused;
    int idx;
    char* key;
    MapIterate(controls, key, ctrl, idx,
        if (idx > 2 && strcmp(key, "button_apply")) EnableControl(ctrl, FALSE);
    );
    MapIterate(texts, key, unused, idx,
        if (idx > 0) EnableText(idx, FALSE);
    );

    HWND app;
    MapGet(controls, "button_apply", app);
    EnableControl(app, TRUE);

    RedrawWindow(mainn, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    
}

void InitWnd() {
    int text1 = Control("label_rnbtsk", WC_TEXT, L"RainbowTaskbar Editor", 0, 10, 10, 0, 0);
    HWND confview = Control("treeview_conf", WC_TREEVIEW, L"Tree", 0, 10, 40, 500, 300);
    TreeView_SetBkColor(confview, RGB(20, 20, 20));
    TreeView_SetTextColor(confview, RGB(200, 200, 200));
    
    HWND remove = Control("button_remove", WC_BUTTON, L"x", 0, 520, 40, 25, 25);
    HWND add = Control("button_add", WC_BUTTON, L"+", 0, 520, 70, 25, 25);


    // common for every effect (duh)
    HWND typepicker = Control("combobox_type", WC_COMBOBOX, L"", CBS_DROPDOWNLIST, 10, 350, 150, 25);
    //ShowWindow(typepicker, FALSE);
    ComboBox_AddString(typepicker, L"Color");
    ComboBox_AddString(typepicker, L"Transparency");
    ComboBox_AddString(typepicker, L"Image");
    ComboBox_AddString(typepicker, L"Delay");
    ComboBox_AddString(typepicker, L"Randomize");
    ComboBox_AddString(typepicker, L"Border radius");

    int helper0 = Control("label_helper0", WC_TEXT, L"- Unknown", 0, 170, 355, 0, 0);

    /*
    char prefix;
	int time;
	BYTE r;
	BYTE g;
	BYTE b;
	char effect[256];
	int effect_1;
	int effect_2;
	int effect_3;
	int effect_4;
	int effect_5;
	int effect_6;
	char effect_7[128];
	char full_line[512];
    */

    // i,w,c - common; t - replaced by combo box;
    HWND time = Control("edit_time", WC_EDIT, L"", 0, 10, 380, 100, 25);
    HWND alphapicker = Control("combobox_time", WC_COMBOBOX, L"", CBS_DROPDOWNLIST, 10, 380, 100, 25);
    ComboBox_AddString(alphapicker, L"Taskbar");
    ComboBox_AddString(alphapicker, L"RnbTsk");
    ComboBox_AddString(alphapicker, L"Both");
    ComboBox_AddString(alphapicker, L"Blur");
    HWND radius = Control("slider_radius", TRACKBAR_CLASS, L"Radius", TBS_HORZ, 10, 380, 100, 25);

    int helper1 = Control("label_helper1", WC_TEXT, L"HELPER TEXT HERE!", 0, 120, 385, 0, 0);
    

    // c - replaced by button ; (i - common ; t - replaced by slider)
    HWND r = Control("edit_r", WC_EDIT, L"", 0, 10, 410, 125, 25);
    HWND colorpick1 = Control("button_colorpick1", WC_BUTTON, L"Color 1", 0, 10, 410, 125, 25);
    HWND alpha = Control("slider_alpha", TRACKBAR_CLASS, L"Alpha", TBS_HORZ, 10, 410, 125, 25);

    int helper2 = Control("label_helper2", WC_TEXT, L"HELPER TEXT HERE!", 0, 140, 415, 0, 0);

    // c - drop down; i - choose file
    HWND effpicker = Control("combobox_effect", WC_COMBOBOX, L"", CBS_DROPDOWNLIST, 10, 440, 150, 25);
    ComboBox_AddString(effpicker, L"Solid");
    ComboBox_AddString(effpicker, L"Fade in solid");
    ComboBox_AddString(effpicker, L"Gradient");
    ComboBox_AddString(effpicker, L"Fade in gradient");
    HWND filepick = Control("button_pickfile", WC_BUTTON, L"Choose file", 0, 10, 440, 150, 25);
    int helper3 = Control("label_helper3", WC_TEXT, L"HELPER TEXT HERE!", 0, 170, 445, 0, 0);

    // c(fade),i - common ; c(grad,fgrd) - button;
    HWND eff1 = Control("edit_effect1", WC_EDIT, L"", 0, 10, 470, 100, 25);
    HWND colorpick2 = Control("button_colorpick2", WC_BUTTON, L"Color 2", 0, 10, 470, 100, 25);
    int helper4 = Control("label_helper4", WC_TEXT, L"HELPER TEXT HERE!", 0, 120, 475, 0, 0);

    // c(fgrd) - common; i - common,optional 
    HWND eff2 = Control("edit_effect2", WC_EDIT, L"", 0, 10, 500, 100, 25);
    int helper5 = Control("label_helper5", WC_TEXT, L"HELPER TEXT HERE!", 0, 115, 505, 0, 0);


    Control_HideAll();

    HWND apply = Control("button_apply", WC_BUTTON, L"Apply", 0, 480, 552, 100, 25);
    SetWindowLong(apply, GWL_EXSTYLE, GetWindowLong(apply, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
    

    //
    cfg = malloc(2048 * sizeof(rtcfg_step));
    ConfigParser(cfg);

    ConfView_Init(confview);
}

HTREEITEM selected;
rtcfg_step selstep;
int selint;

void ConfView_Init(HWND confview) {
    TreeView_DeleteItem(confview, NULL /* all */);

    for (int i = 0; i < cfg->len; i++) {
        rtcfg_step cur = cfg->steps[i];

        TVINSERTSTRUCT tvis = { 0 };
        tvis.hParent = NULL;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        char* line = malloc(strlen(cur.full_line)+1);
        strcpy(line, cur.full_line);
        if (cur.full_line[strlen(cur.full_line) - 1] == '\n') {
            memcpy(line, cur.full_line, strlen(cur.full_line) - 3);
            line[strlen(cur.full_line) - 2] = '\0';
        }

        wchar_t* buf = malloc(513 * sizeof(wchar_t));
        size_t discard;
        mbstowcs(buf, line, 512);
        tvis.item.pszText = buf;
        tvis.item.cchTextMax = 512;
        TreeView_InsertItem(confview, &tvis);

        free(buf);
        free(line);
    }
}

int ConfView_Index(HTREEITEM item){
    int index = 0;
    BOOL found = FALSE;
    HWND confview;
    MapGet(controls, "treeview_conf", confview);

    HTREEITEM root = TreeView_GetNextItem(confview, NULL, TVGN_ROOT);
    HTREEITEM cur = root;

    while (!found) {
        if (cur == item) {found = TRUE; break;}
        cur = TreeView_GetNextItem(confview, cur, TVGN_NEXT);
        if (cur == NULL) break;
        index++;
    }
    return found ? index : -1;
}

POINT Point_FromString(LPWSTR str) {
    POINT pt = { 0 };
    wchar_t* buf = malloc(1026);
    wchar_t* tok; 
    
    tok = wcstok(str, L"x", buf);
    if(tok != NULL) swscanf(tok, L"%i", &pt.x);

    if (tok != NULL) tok = wcstok(NULL, L"x", buf);
    if (tok != NULL) swscanf(tok, L"%i", &pt.y);

    free(buf);

    return pt;
}

void ConfView_OnSelect() {
    HWND ctrl;
    MapGet(controls, "treeview_conf", ctrl);

    selected = TreeView_GetSelection(ctrl);
    TCHAR buffer[512];

    TVITEM item;
    item.hItem = selected;
    item.mask = TVIF_TEXT;
    item.cchTextMax = 512;
    item.pszText = buffer;
    TreeView_GetItem(ctrl, &item);
    selint = ConfView_Index(selected);

    CHAR buffer2[512];

    wcstombs(buffer2, buffer, 512);

    Config_ParseLine(buffer2);

    rgbCurr = RGB(selstep.r, selstep.g, selstep.b);

    Control_HideAll();
    HWND picker;
    MapGet(controls, "combobox_type", picker);
    EnableControl(picker, TRUE);

    int idx;
    MapGetIdx(texts, "label_helper0", idx);
    TextModify(idx, L"- Unknown");
    EnableText(idx, TRUE);

    XY = FALSE;

    switch (selstep.prefix) {
        case 'r':
        {
            MapGetIdx(texts, "label_helper0", idx);
            TextModify(idx, L"- Randomize next colors");

            ComboBox_SelectString(picker, 0, L"Randomize");
            break;
        }
        case 'b':
        {
            MapGetIdx(texts, "label_helper0", idx);
            TextModify(idx, L"- Set taskbar border radius");

            ComboBox_SelectString(picker, 0, L"Border radius");

            HWND radius;
            MapGet(controls, "slider_radius", radius);

            SendMessage(radius, TBM_SETRANGEMIN, TRUE, 0);
            SendMessage(radius, TBM_SETRANGEMAX, TRUE, 64);
            SendMessage(radius, TBM_SETPOS, TRUE, selstep.time);

            MapGetIdx(texts, "label_helper1", idx);
            TextModify(idx, L"- Border radius in pixels");
            EnableText(idx, TRUE);

            EnableControl(radius, TRUE);

            break;
        }
        case 'w':
        {
            MapGetIdx(texts, "label_helper0", idx);
            TextModify(idx, L"- Sleep for n milliseconds");


            ComboBox_SelectString(picker, 0, L"Delay");
            MapGetIdx(texts, "label_helper1", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"ms - Time in milliseconds to wait for");

            HWND input;
            MapGet(controls, "edit_time", input);

            LPWSTR delay = malloc(17*sizeof(WCHAR));
            wsprintfW(delay, L"%i", selstep.time);
            
            Edit_SetText(input, delay);
            free(delay);

            EnableControl(input, TRUE);
            break;
        }
        case 't':
            MapGetIdx(texts, "label_helper0", idx);
            TextModify(idx, L"- Set opacity or blur of taskbar");

            ComboBox_SelectString(picker, 0, L"Transparency");
            MapGetIdx(texts, "label_helper1", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"- Feature to modify");

            HWND input;
            MapGet(controls, "combobox_time", input);

            LPWSTR who = L"";
            if (selstep.time == 0) { who = L"Taskbar"; selstep.time = 1; }
            if (selstep.time == 2) who = L"RnbTsk";
            if (selstep.time == 1) who = L"Taskbar";
            if (selstep.time == 3) who = L"Both";
            if (selstep.time == 4) who = L"Blur";
            ComboBox_SelectString(input, 0, who);

            HWND slider;
            MapGet(controls, "slider_alpha", slider);
            EnableControl(slider, TRUE);

            MapGetIdx(texts, "label_helper2", idx);
            EnableText(idx, TRUE);
            TextModify(idx, selstep.time == 4 ? L"- Enable or disable blur" : L"- Set opacity");

            SendMessage(slider, TBM_SETRANGEMIN, TRUE, 0);
            SendMessage(slider, TBM_SETRANGEMAX, TRUE, selstep.time == 4 ? 1 : 255);
            SendMessage(slider, TBM_SETPOS, TRUE, selstep.r);


            EnableControl(input, TRUE);
            break;
        case 'c':
            MapGetIdx(texts, "label_helper0", idx);
            TextModify(idx, L"- Set color effect");

            ComboBox_SelectString(picker, 0, L"Color");
            MapGetIdx(texts, "label_helper1", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"ms - Effect time in milliseconds");

            HWND in1;
            MapGet(controls, "edit_time", in1);
            LPWSTR txt = malloc(17 * sizeof(wchar_t));
            wsprintf(txt, L"%i", selstep.time);
            Edit_SetText(in1, txt);
            free(txt);

            HWND BTN;
            MapGet(controls, "button_colorpick1", BTN);

            MapGetIdx(texts, "label_helper2", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"- Color 1 picker");

            HWND pick2;
            MapGet(controls, "combobox_effect", pick2);
            switchs(selstep.effect) {
                cases("none") {
                    ComboBox_SelectString(pick2, 0, L"Solid");
                    breaks;
                }
                cases("fade") {
                    ComboBox_SelectString(pick2, 0, L"Fade in solid");
                    LPWSTR amuz1 = malloc(17 * sizeof(WCHAR));
                    swprintf(amuz1, 16, L"%i", selstep.effect_1);

                    HWND ef1;
                    MapGet(controls, "edit_effect1", ef1);

                    Edit_SetText(ef1, amuz1);

                    int idx1;
                    MapGetIdx(texts, "label_helper4", idx1);

                    TextModify(idx1, L"- Fade duration in milliseconds");

                    EnableText(idx1, TRUE);
                    EnableControl(ef1, TRUE);

                    free(amuz1);

                    breaks;
                }
                cases("grad") {
                    ComboBox_SelectString(pick2, 0, L"Gradient");
                    

                    HWND pickc2;
                    MapGet(controls, "button_colorpick2", pickc2);
                    EnableControl(pickc2, TRUE);
                    rgbCurr2 = RGB(selstep.effect_1, selstep.effect_2, selstep.effect_3);
                    breaks;
                }
                cases("fgrd") {
                    ComboBox_SelectString(pick2, 0, L"Fade in gradient");
                    LPWSTR amuz1 = malloc(17*sizeof(WCHAR));
                    swprintf(amuz1, 16, L"%i", selstep.effect_4);
                    

                    HWND pickc2,ef1;
                    MapGet(controls, "button_colorpick2", pickc2);
                    MapGet(controls, "edit_effect2", ef1);

                    Edit_SetText(ef1, amuz1);

                    free(amuz1);
                    int idx1,idx2;
                    MapGetIdx(texts, "label_helper4", idx1);
                    MapGetIdx(texts, "label_helper5", idx2);

                    TextModify(idx1, L"- Color 2 picker");
                    TextModify(idx2, L"- Fade duration in milliseconds");

                    EnableText(idx1, TRUE);
                    EnableText(idx2, TRUE);

                    EnableControl(pickc2, TRUE);
                    EnableControl(ef1, TRUE);
                    rgbCurr2 = RGB(selstep.effect_1, selstep.effect_2, selstep.effect_3);
                    breaks;
                }
            }
            MapGetIdx(texts, "label_helper3", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"- Color effect");
            
            EnableControl(in1, TRUE);
            EnableControl(BTN, TRUE);
            EnableControl(pick2, TRUE);
            break;
        case 'i':
        {
            MapGetIdx(texts, "label_helper0", idx);
            TextModify(idx, L"- Display bitmap image");


            ComboBox_SelectString(picker, 0, L"Image");

            MapGetIdx(texts, "label_helper1", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"- XY coordinates for image");
            XY = TRUE;

            HWND input;
            MapGet(controls, "edit_time", input);

            LPWSTR xy1 = malloc(33 * sizeof(WCHAR));
            wsprintfW(xy1, L"%ix%i", selstep.time, selstep.r);

            Edit_SetText(input, xy1);
            free(xy1);
            EnableControl(input, TRUE);

            MapGetIdx(texts, "label_helper2", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"- Size on taskbar. Leave empty for full");

            HWND input2;
            MapGet(controls, "edit_r", input2);

            LPWSTR xy2 = malloc(33 * sizeof(WCHAR));
            wsprintfW(xy2, L"%ix%i", selstep.g, selstep.b);

            Edit_SetText(input2, xy2);
            free(xy2);
            EnableControl(input2, TRUE);

            HWND pickbut;
            MapGet(controls, "button_pickfile", pickbut);

            MapGetIdx(texts, "label_helper3", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"- Pick BMP image");

            EnableControl(pickbut, TRUE);

            HWND alpha;
            MapGet(controls, "edit_effect1", alpha);

            LPWSTR aph = malloc(33 * sizeof(WCHAR));
            wsprintfW(aph, L"%i", selstep.effect_1);

            Edit_SetText(alpha, aph);
            free(aph);
            MapGetIdx(texts, "label_helper4", idx);
            EnableText(idx, TRUE);
            TextModify(idx, L"- Image alpha (used to mix with color effects)");

            EnableControl(alpha, TRUE);



            break;
        }
        default:
            EnableWindow(picker, FALSE);
    }

    RedrawWindow(mainn, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void Config_ParseLine(char* ln) {
    sscanf(ln, "%c %i %i %i %i %s %i %i %i %i %i %i %127s", &selstep.prefix, &selstep.time, &selstep.r, &selstep.g, &selstep.b, &selstep.effect, &selstep.effect_1, &selstep.effect_2, &selstep.effect_3, &selstep.effect_4, &selstep.effect_5, &selstep.effect_6, &selstep.effect_7);
}

void Config_MakeLn(int idx) {
    rtcfg_step* ss = &cfg->steps[idx];
    switch (ss->prefix) {
    case 't':
        sprintf(ss->full_line, "t %i %i", ss->time, ss->r);
        break;
    case 'w':
        sprintf(ss->full_line, "w %i", ss->time);
        break;
    case 'i':
        sprintf(ss->full_line, "i %i %i %i %i %s %i %i %i", ss->time, ss->r, ss->g, ss->b, ss->effect, ss->effect_1, ss->effect_2, ss->effect_3);
        break;
    case 'c':
        switchs(ss->effect) {
            cases("none") {
                sprintf(ss->full_line, "c %i %i %i %i none", ss->time, ss->r, ss->g, ss->b);
                breaks;
            }
            cases("fade") {
                sprintf(ss->full_line, "c %i %i %i %i fade %i", ss->time, ss->r, ss->g, ss->b, ss->effect_1);
                breaks;
            }
            cases("grad") {
                sprintf(ss->full_line, "c %i %i %i %i grad %i %i %i", ss->time, ss->r, ss->g, ss->b, ss->effect_1, ss->effect_2, ss->effect_3);
                breaks;
            }
            cases("fgrd") {
                sprintf(ss->full_line, "c %i %i %i %i fgrd %i %i %i %i", ss->time, ss->r, ss->g, ss->b, ss->effect_1, ss->effect_2, ss->effect_3, ss->effect_4);
                breaks;
            }
            defaults{
                sprintf(ss->full_line, "c %i %i %i %i none", ss->time, ss->r, ss->g, ss->b);
                breaks;
            }

        }
        break;
    case 'r':
        sprintf(ss->full_line, "r");
        break;
    }
}

void Config_MakeFullLine() {

    //reference
    //sprintf(full_line, "%c %i %i %i %i %s %i %i %i %i %i %i %s", step.prefix, step.time, step.r, step.g, step.b, step.effect, step.effect_1, step.effect_2, step.effect_3, step.effect_4, step.effect_5, step.effect_6, step.effect_7);

    switch (selstep.prefix) {
        case 't':
            sprintf(selstep.full_line, "t %i %i", selstep.time, selstep.r);
            break;
        case 'w':
            sprintf(selstep.full_line, "w %i", selstep.time);
            break;
        case 'i':
            sprintf(selstep.full_line, "i %i %i %i %i %s %i %i %i", selstep.time, selstep.r, selstep.g, selstep.b, selstep.effect, selstep.effect_1, selstep.effect_2, selstep.effect_3);
            break;
        case 'c':
            switchs(selstep.effect) {
                cases("none") {
                    sprintf(selstep.full_line, "c %i %i %i %i none", selstep.time, selstep.r, selstep.g, selstep.b);
                    breaks;
                }
                cases("fade") {
                    sprintf(selstep.full_line, "c %i %i %i %i fade %i", selstep.time, selstep.r, selstep.g, selstep.b, selstep.effect_1);
                    breaks;
                }
                cases("grad") {
                    sprintf(selstep.full_line, "c %i %i %i %i grad %i %i %i", selstep.time, selstep.r, selstep.g, selstep.b, selstep.effect_1, selstep.effect_2, selstep.effect_3);
                    breaks;
                }
                cases("fgrd") {
                    sprintf(selstep.full_line, "c %i %i %i %i fgrd %i %i %i %i", selstep.time, selstep.r, selstep.g, selstep.b, selstep.effect_1, selstep.effect_2, selstep.effect_3, selstep.effect_4);
                    breaks;
                }
                defaults{
                    sprintf(selstep.full_line, "c %i %i %i %i none", selstep.time, selstep.r, selstep.g, selstep.b);
                    breaks;
                }

            }
            break;
        case 'r':
            sprintf(selstep.full_line, "r");
            break;
        case 'b':
            sprintf(selstep.full_line, "b %i", selstep.time);
            break;
    }
    HWND confview;
    MapGet(controls, "treeview_conf", confview);

    int idx = ConfView_Index(selected);
    cfg->steps[idx] = selstep;

    LPWSTR str = malloc(513 * sizeof(wchar_t));
    mbstowcs(str, selstep.full_line, 512);

    TVITEM item;
    item.hItem = selected;
    item.mask = TVIF_TEXT;
    item.pszText = str;
    item.cchTextMax = 512;

    TreeView_SetItem(confview, &item);
    free(str);

}

void Config_Modify(char* what, void* to) {
    switchs(what) {
        cases("rgb") {

            COLOR* color = (COLOR*)to;
            selstep.r = color->R;
            selstep.g = color->G;
            selstep.b = color->B;

            Config_MakeFullLine();
            breaks;
        }
        cases("type") {
            char* c = (char*)to;
            memset(&selstep, 0, sizeof(rtcfg_step));
            selstep.prefix = *c;
            Config_MakeFullLine();
            breaks;
        }
        cases("time") {
            int* c = (int*)to;
            selstep.time = *c;
            Config_MakeFullLine();
            breaks;
        }
        cases("r") {
            int* r = (int*)to;
            selstep.r = *r;
            Config_MakeFullLine();
            breaks;
        }
        cases("g") {
            int* g = (int*)to;
            selstep.g = *g;
            Config_MakeFullLine();
            breaks;
        }
        cases("b") {
            int* b = (int*)to;
            selstep.b = *b;
            Config_MakeFullLine();
            breaks;
        }
        cases("effect") {
            strcpy(selstep.effect, (char*)to);
            Config_MakeFullLine();
            breaks;
        }
        cases("effect1") {
            int* b = (int*)to;
            selstep.effect_1 = *b;
            Config_MakeFullLine();
            breaks;
        }
        cases("effect2") {
            int* b = (int*)to;
            selstep.effect_2 = *b;
            Config_MakeFullLine();
            breaks;
        }
        cases("rgb2") {
            COLOR* color = (COLOR*)to;
            selstep.effect_1 = color->R;
            selstep.effect_2 = color->G;
            selstep.effect_3 = color->B;

            Config_MakeFullLine();
            breaks;
        }
        cases("effect4") {
            int* b = (int*)to;
            selstep.effect_4 = *b;
            Config_MakeFullLine();
            breaks;
        }
        cases("effect5") { // unused
            int* b = (int*)to;
            selstep.effect_5 = *b;
            Config_MakeFullLine();
            breaks;
        }
        cases("xycord1") {
            POINT* xycord1 = (POINT*)to;
            selstep.time = xycord1->x;
            selstep.r = xycord1->y;

            Config_MakeFullLine();
            breaks;
        }
        cases("xycord2") {
            POINT* xycord2 = (POINT*)to;
            selstep.g = xycord2->x;
            selstep.b = xycord2->y;

            Config_MakeFullLine();
            breaks;
        }
        cases("xycord3") { // unused
            POINT* xycord3 = (POINT*)to;
            selstep.effect_2 = xycord3->x;
            selstep.effect_3 = xycord3->y;

            Config_MakeFullLine();
            breaks;
        }

    }
    cfg->steps[selint] = selstep;

}

void Config_Pop(int idx) {
    if (idx < cfg->len-1) {
        int move = idx;
        memset(&cfg->steps[idx], 0, sizeof(rtcfg_step));
        while (move < cfg->len) {
            memcpy(&cfg->steps[move], &cfg->steps[move+1], sizeof(rtcfg_step));
            move++;
        }
    }
    else {
        memset(&cfg->steps[idx], 0, sizeof(rtcfg_step));
    }
    cfg->len--;
}
void Config_Insert(int idx, rtcfg_step what) {
    if (idx == 0 && cfg->len == 0) {
        memcpy(&cfg->steps[idx], &what, sizeof(rtcfg_step));
        cfg->len += 1;
        return;
    }
    if (idx < cfg->len) {
        

        for (int i = cfg->len; i >= idx; i--)
            memcpy(&cfg->steps[i], &cfg->steps[i-1], sizeof(rtcfg_step));
        
        memcpy(&cfg->steps[idx], &what, sizeof(rtcfg_step));
    }
    else {
        memcpy(&cfg->steps[idx], &what, sizeof(rtcfg_step));
    }
    cfg->len++;
}

void Config_Apply() {
    char* appdata = getenv("APPDATA");
    char* confpath[512];
    sprintf(confpath, "%s\\rnbconf.txt", appdata);
    FILE* conf = fopen(confpath, "w");
    fprintf(conf, "# RNBTSK CFG\n\n");
    fclose(conf);
    conf = fopen(confpath, "a");
    for (int i = 0; i < cfg->len; i++) {
        Config_MakeLn(i);
        fprintf(conf, "%s\n", cfg->steps[i].full_line);
    }
    fprintf(conf, "\n\n# EOF");
    fclose(conf);

    NewConf(cfg);
}

void Remove_OnClick() {
    if (cfg->len <= 0) {
        cfg->len = 0;
        return;
    }
    if (ConfView_Index(selected) == -1) return;
    
    
    
    Config_Pop(selint);

    HWND del;
    MapGet(controls, "treeview_conf", del);
    TreeView_DeleteItem(del, selected ? selected : NULL /* all */);

    if (cfg->len == 0) {
        HWND picker;
        MapGet(controls, "treeview_conf", picker);

        Control_HideAll();
        
    }
}

void Add_OnClick() {
    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = NULL;
    tvis.hInsertAfter = selected ? selected : TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    tvis.item.pszText = L"c 100 0 0 0 none";
    tvis.item.cchTextMax = 512;
    HWND add;
    MapGet(controls, "treeview_conf", add);
    TreeView_InsertItem(add, &tvis);

    rtcfg_step buffer = { 0 };
    buffer.prefix = 'c';
    buffer.time = 100;
    buffer.r = 0;
    buffer.g = 0;
    buffer.b = 0;
    memcpy(buffer.effect, "none", 5);
    Config_Insert(selint+(cfg->len > 0), buffer);


    
}

void ComboBox1_OnModify() {
    char to = 'c';
    int to2 = 0;
    HWND cb;
    MapGet(controls, "combobox_type", cb);

    int index = ComboBox_GetCurSel(cb);
    wchar_t* sel = malloc(17*sizeof(wchar_t));
    ComboBox_GetText(cb, sel, 16);
    

    // C T I D

    switchsw (sel) {
        casesw(L"Color") {
            to = 'c';
            breaks;
        }
        casesw(L"Transparency") {
            to = 't';
            to2 = 1;
            Config_Modify("time", &to2);
            breaks;
        }
        casesw(L"Image") {
            to = 'i';
            breaks;
        }
        casesw(L"Delay") {
            to = 'w';
            breaks;
        }
        casesw(L"Randomize") {
            to = 'r';
            breaks;
        }
        casesw(L"Border radius") {
            to = 'b';
            breaks;
        }
    }
    if (to == selstep.prefix) {
        free(sel);
        return;
    }

    Config_Modify("type", &to);
    ConfView_OnSelect();
    free(sel);
}
void ComboBox2_OnModify() {
    int to = 0;
    int to2 = 0;
    HWND cb;
    MapGet(controls, "combobox_time", cb);

    int index = ComboBox_GetCurSel(cb);
    wchar_t* sel = malloc(17 * sizeof(wchar_t));
    ComboBox_GetText(cb, sel, 16);
    
    switchsw(sel) {
        casesw(L"RnbTsk") {
            to = 2;
            to2 = 255;
            breaks;
        }
        casesw(L"Taskbar") {
            to = 1;
            to2 = 255;
            breaks;
        }
        casesw(L"Both") {
            to = 3;
            to2 = 255;
            breaks;
        }
        casesw(L"Blur") {
            to = 4;
            to2 = 1;
            breaks;
        }
    }

    HWND slider;
    MapGet(controls, "slider_alpha", slider);

    SendMessage(slider, TBM_SETRANGEMIN, TRUE, 0);
    SendMessage(slider, TBM_SETRANGEMAX, TRUE, to == 4 ? 1 : 255);
    SendMessage(slider, TBM_SETPOS, TRUE, to2);

    int idx;
    MapGetIdx(texts, "label_helper2", idx);
    EnableText(idx, TRUE);
    TextModify(idx, to == 4 ? L"- Enable or disable blur" : L"- Set opacity");

    Config_Modify("time", &to);
    Config_Modify("r", &to2);
    //ConfView_OnSelect();
    free(sel);
}

void ComboBox3_OnModify() {
    LPCSTR to = "none";
    int to1 = 100;
    int to2 = 0;
    COLOR to3 = { 0 };
    HWND cb;
    MapGet(controls, "combobox_effect", cb);

    int index = ComboBox_GetCurSel(cb);
    wchar_t* sel = malloc(25 * sizeof(wchar_t));
    ComboBox_GetText(cb, sel, 24);

    switchsw(sel) {
        casesw(L"Solid") {
            to = "none";
            breaks;
        }
        casesw(L"Fade in solid") {
            to = "fade";
            Config_Modify("effect1", &to1);
            //Config_Modify("effect2", &to2); DEPRECATED
            breaks;
        }
        casesw(L"Gradient") {
            to = "grad";
            Config_Modify("rgb2", &to3);
            breaks;
        }
        casesw(L"Fade in gradient") {
            to = "fgrd";
            Config_Modify("rgb2", &to3);
            Config_Modify("effect4", &to1);
            //Config_Modify("effect5", &to2); DEPRECATED
            breaks;
        }
        defaults{
            to = "none";
            breaks;
        }
    }

    Config_Modify("effect", to);
    ConfView_OnSelect();
    free(sel);
}

void ColorPick1_OnClick() {
    rgbCurr = PickColor(0);
    Config_Modify("rgb", &rgbCurr);

    RedrawWindow(mainn, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}
void ColorPick2_OnClick() {
    rgbCurr2 = PickColor(1);
    Config_Modify("rgb2", &rgbCurr2);

    RedrawWindow(mainn, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}
void Slider1_OnModify() {
    if (selstep.prefix == 't') {
        HWND slider;
        MapGet(controls, "slider_alpha", slider);
        int val = SendMessage(slider, TBM_GETPOS, 0, 0);
        Config_Modify("r", &val);
    }
    else {
        HWND slider;
        MapGet(controls, "slider_radius", slider);
        int val = SendMessage(slider, TBM_GETPOS, 0, 0);
        Config_Modify("time", &val);
    }
    
}
void PickFile_OnClick() {
    LPWSTR path = malloc(257*sizeof(WCHAR));
    FileDiag_Open(mainn, path);
    LPCSTR patha = malloc(257);

    wcstombs(patha, path, 256);

    Config_Modify("effect", patha);


    free(path);
    free(patha);

}

void Edit0_OnModify() {
    HWND edit;
    MapGet(controls, "edit_r", edit);

    LPWSTR here = malloc(33 * sizeof(wchar_t));
    Edit_GetText(edit, here, 32);
    POINT XYCORD2 = Point_FromString(here);

    Config_Modify("xycord2", &XYCORD2);

    free(here);
}
void Edit1_OnModify() {
    int val = 0;
    HWND edit;
    MapGet(controls, "edit_time", edit);

    LPWSTR here = malloc(33 * sizeof(wchar_t));
    if (!XY) {
        Edit_GetText(edit, here, 32);
        swscanf(here, L"%i", &val);

        Config_Modify("time", &val);
    }
    else {
        Edit_GetText(edit, here, 32);
        POINT XYCORD1 = Point_FromString(here);

        Config_Modify("xycord1", &XYCORD1);
    }
    free(here);
}
void Edit2_OnModify() {
    int val = 0;
    HWND edit;
    MapGet(controls, "edit_effect1", edit);

    LPWSTR here = malloc(33 * sizeof(wchar_t));

    Edit_GetText(edit, here, 32);
    swscanf(here, L"%i", &val);

    Config_Modify("effect1", &val);
    
    free(here);
}
void Edit3_OnModify() {
    int val = 0;
    HWND edit;
    MapGet(controls, "edit_effect2", edit);

    LPWSTR here = malloc(33 * sizeof(wchar_t));

    if (!XY) {
        Edit_GetText(edit, here, 32);
        swscanf(here, L"%i", &val);

        Config_Modify("effect4", &val);
    }
    else {
        Edit_GetText(edit, here, 32);
        POINT XYCORD1 = Point_FromString(here);

        Config_Modify("xycord3", &XYCORD1);
    }

    
    free(here);
}

void minimize() {
    // unused
}

#define MENU0 0xAD20 + 0
#define MENU1 0xAD20 + 1
#define MENU2 0xAD20 + 2
#define MENU3 0xAD20 + 3
#define MENU4 0xAD20 + 4
#define MENU5 0xAD20 + 5

#define SUB0 0xAE20 + 0
#define SUB1 0xAE20 + 1
#define SUB2 0xAE20 + 2
#define SUB3 0xAE20 + 3
#define SUB4 0xAE20 + 4

void Example(char* ex) {
    char* appdata = getenv("APPDATA");
    char* confpath[512];
    sprintf(confpath, "%s\\rnbconf.txt", appdata);
    FILE* fconfig = fopen(confpath, "w");
    fprintf(fconfig, "%s", ex);
    fclose(fconfig);
    ConfigParser(rcfg);
    ConfigParser(cfg);
    NewConf(rcfg);
    HWND confview;
    MapGet(controls, "treeview_conf", confview);
    ConfView_Init(confview);
}

LRESULT CALLBACK GUIProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    
    switch (uMsg)
    {
    case WM_USER_SHELLICON:
    {
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP:
        {

            ShowWindow(hwnd, TRUE);
            break;

        }
        case WM_RBUTTONUP:
        {
            HMENU menu = CreatePopupMenu();
            HMENU submenu = CreatePopupMenu();
            AppendMenu(submenu, 0, SUB0, L"Rainbow");
            AppendMenu(submenu, 0, SUB1, L"Pulse");
            AppendMenu(submenu, 0, SUB2, L"Ambient");
            AppendMenu(submenu, 0, SUB4, L"Randomize");
            AppendMenu(submenu, 0, SUB3, L"Static");


            AppendMenu(menu, 0, MENU0, L"Open config file");
            AppendMenu(menu, MF_POPUP, submenu, L"Config examples");
            AppendMenu(menu, 0, MENU3, L"Open project page");
            AppendMenu(menu, 0, MENU5, L"Donate");
            MENUITEMINFO real;
            HBITMAP bmp;
            bmp = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BITMAP2), IMAGE_BITMAP, 16, 16, 0);
            
            real.cbSize = sizeof(MENUITEMINFO);
            real.fMask = MIIM_BITMAP;
            real.hbmpItem = bmp;
            SetMenuItemInfo(menu, MENU5, 0, &real);
            AppendMenu(menu, STARTUP ? MF_CHECKED : MF_UNCHECKED, MENU1, L"Run at startup");
            AppendMenu(menu, 0, MENU2, L"Exit");

            POINT p;
            GetCursorPos(&p);
            TrackPopupMenu(menu, 0, p.x, p.y+8, 0, hwnd, 0);

            DestroyMenu(menu);
            break;
            
        }
        }
        break;
    }
    case WM_NEWTEXT: // custom message
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        break;
    case WM_CREATE:
        
        SetActiveWindow(hwnd);
        break;
    case WM_NOTIFY:
        switch (((LPNMHDR)lParam)->code)
        {
        case TVN_SELCHANGED:
            ConfView_OnSelect((LPNMHDR)lParam);
            break;
        case TVN_KEYDOWN:
            switch (((NMTVKEYDOWN*)lParam)->wVKey) {
                case VK_DELETE:
                    Remove_OnClick();
                    break;
                case VK_INSERT:
                    Add_OnClick();
                    break;
            }
            break;
        }
        
        

        
        break;
    case WM_HSCROLL:
        Slider1_OnModify();
        break;
    case WM_COMMAND:
    {

        switch (wParam) {
            case MENU0: // cfg file
            {
                system("start %appdata%\\rnbconf.txt");

                break;
            }
            case MENU1: // startup
            {
                STARTUP = !STARTUP;

                char* appdata = getenv("APPDATA");
                char* confpath[512];
                sprintf(confpath, "%s\\rnbconf.txt", appdata);

                FILE* fconfig = fopen(confpath, "r");
                TCHAR thisfile[MAX_PATH];
                GetModuleFileName(NULL, thisfile, MAX_PATH);

                HKEY hkey = NULL;
                RegCreateKey(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), &hkey);
                if (STARTUP) {
                    RegSetValueEx(hkey, _T("RainbowTaskbar"), 0, REG_SZ, thisfile, (_tcslen(thisfile) + 1) * sizeof(TCHAR));
                }
                else {
                    RegDeleteValue(hkey, _T("RainbowTaskbar"));
                }

                

                RegCloseKey(hkey);

                break;
            }
            case MENU2: // quit
            {
                OnDestroy();
                DestroyWindow(hwnd);
                break; // ?
            }
            case MENU3:
            {
                system("explorer \"https://github.com/ad2017gd/RainbowTaskbar\"");
                break;
            }
            case SUB0:
            {
                char example1[] = 
                    "t 4\n"
                    "t 2 200\n"
                    "\n"
                    "c 1 255 0 0 fgrd 255 154 0 500\n"
                    "c 1 255 154 0 fgrd 208 222 33 500\n"
                    "c 1 208 222 33 fgrd 79 220 74 500\n"
                    "c 1 79 220 74 fgrd 63 218 216 500\n"
                    "c 1 63 218 216 fgrd 47 201 226 500\n"
                    "c 1 47 201 226 fgrd 28 127 238 500\n"
                    "c 1 28 127 238 fgrd 95 21 242 500\n"
                    "c 1 95 21 242 fgrd 186 12 248 500\n"
                    "c 1 186 12 248 fgrd 251 7 217 500\n"
                    "c 1 251 7 217 fgrd 255 0 0 500";
                Example(example1);
                break;
            }
            case SUB1:
            {
                char example2[] =
                    "t 4\n"
                    "t 2 180\n"
                    "t 1 230\n"
                    "\n"
                    "c 750 255 0 180 fgrd 0 200 255 400\n"
                    "c 100 255 255 255 grad 255 255 255\n"
                    "c 750 0 200 255 fgrd 255 0 180 400\n"
                    "c 100 255 255 255 grad 255 255 255\n";
                Example(example2);
                break;
            }
            case SUB2:
            {
                char example3[] =
                    "t 4 1\n"
                    "t 3 201\n"
                    "c 1500 2 40 104 fgrd 0 165 253 4000\n"
                    "c 1500 0 165 253 fgrd 2 40 104 4000\n";
                Example(example3);
                break;
            }
            case SUB3:
            {
                char example4[] =
                    "t 4 0\n"
                    "t 3 224\n"
                    "c 99999999 30 120 219 none\n";
                Example(example4);
                break;
            }
            case SUB4:
            {
                char example5[] =
                    "t 4 1\n"
                    "t 3 201\n"
                    "r\n"
                    "c 1 0 0 0 fgrd 0 0 0 1000\n"
                    "r\n"
                    "c 1 0 0 0 fgrd 0 0 0 1000\n";
                Example(example5);
                break;
            }
            case MENU5:
            {
                system("explorer \"https://paypal.me/ad2k17\"");
                break;
            }
        }
        switch (HIWORD(wParam)) {

        case CBN_SELCHANGE:
        {
            char* key;
            MapFind(controls, lParam, key);

            switchs(key) {
                cases("combobox_type") {
                    ComboBox1_OnModify();
                    breaks;
                }
                cases("combobox_time") {
                    ComboBox2_OnModify();
                    breaks;
                }
                cases("combobox_effect") {
                    ComboBox3_OnModify();
                    breaks;
                }
            }


            break;
        }
        case EN_UPDATE:
        {
            char* key;
            MapFind(controls, lParam, key);

            switchs(key) {
                cases("edit_r") {
                    Edit0_OnModify();
                    breaks;
                }
                cases("edit_time") {
                    Edit1_OnModify();
                    breaks;
                }
                cases("edit_effect1") {
                    Edit2_OnModify();
                    breaks;
                }
                cases("edit_effect2") {
                    Edit3_OnModify();
                    breaks;
                }
            }
            break;
        }
        case BN_CLICKED:
        {
            char* key;
            MapFind(controls, lParam, key);

            switchs(key) {
                cases("button_remove") {
                    Remove_OnClick();
                    breaks;
                }
                cases("button_add") {
                    Add_OnClick();
                    breaks;
                }
                cases("button_colorpick1") {
                    ColorPick1_OnClick();
                    breaks;
                }
                cases("button_colorpick2") {
                    ColorPick2_OnClick();
                    breaks;
                }
                cases("button_pickfile") {
                    PickFile_OnClick();
                    breaks;
                }
                cases("button_apply") {
                    Config_Apply();
                    breaks;
                }
            }
            break;
        }

        }
        
        break;
    }
    case WM_SIZE:
    {

        SendMessage(hwnd, WM_PRINT, (WPARAM)NULL, PRF_NONCLIENT);
        break;
    }
    case WM_SIZING:
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, FALSE);
        return 0;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkColor(hdcStatic, bgc);
        SetTextColor(hdcStatic, RGB(255, 255, 255));
        SetDCBrushColor(hdcStatic, RGB(255,255,255));
        
        return (INT_PTR)transparent;
    }
    case WM_DESTROY:
        if (transparent) DeleteObject(transparent);

        NOTIFYICONDATA sh = { 0 };
        sh.uID = 0xad2017;
        sh.hWnd = hwnd;
        Shell_NotifyIcon(NIM_DELETE, &sh);

        HWND hwd_;
        if (hwd_ = FindWindow(L"RnbTskWnd", NULL)) {
            DWORD procid;
            GetWindowThreadProcessId(hwd_, &procid);
            HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, procid);
            DestroyWindow(hwd_);
            TerminateProcess(proc, 0);
        }
        PostQuitMessage(0);
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        FillRect(hdc, &ps.rcPaint, transparent);
        //
        SelectObject(hdc, (LPARAM)guiFont);
        SetBkColor(hdc, bgc);
        SetTextColor(hdc, RGB(200, 200, 200));

        if (selstep.prefix == 'c') {
            HBRUSH c1 = CreateSolidBrush(RGB(selstep.r, selstep.g, selstep.b));
            switchs(selstep.effect) {
                cases("none") {
                    RECT rc;
                    rc = ps.rcPaint;
                    rc.bottom = 10;

                    FillRect(hdc, &rc, c1);
                        breaks;
                }
                cases("fade") {
                    RECT rc;
                    rc = ps.rcPaint;
                    rc.bottom = 10;

                    FillRect(hdc, &rc, c1);
                    breaks;
                }
                cases("grad") {

                    RECT rc;
                    rc = ps.rcPaint;
                    rc.bottom = 10;

                    TRIVERTEX        vertex[2];
                    GRADIENT_RECT    gRect = { 0 };
                    vertex[0].x = 0;
                    vertex[0].y = 0;
                    vertex[0].Red = selstep.r << 8;
                    vertex[0].Green = selstep.g << 8;
                    vertex[0].Blue = selstep.b << 8;
                    vertex[0].Alpha = 0xFFFF;

                    vertex[1].x = rc.right;
                    vertex[1].y = rc.bottom;
                    vertex[1].Red = selstep.effect_1 << 8;
                    vertex[1].Green = selstep.effect_2 << 8;
                    vertex[1].Blue = selstep.effect_3 << 8;
                    vertex[1].Alpha = 0xFFFF;

                    gRect.UpperLeft = 0;
                    gRect.LowerRight = 1;
                    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
                    breaks;
                }
                cases("fgrd") {

                    RECT rc;
                    rc = ps.rcPaint;
                    rc.bottom = 10;

                    TRIVERTEX        vertex[2];
                    GRADIENT_RECT    gRect = { 0 };
                    vertex[0].x = 0;
                    vertex[0].y = 0;
                    vertex[0].Red = selstep.r << 8;
                    vertex[0].Green = selstep.g << 8;
                    vertex[0].Blue = selstep.b << 8;
                    vertex[0].Alpha = 0xFFFF;

                    vertex[1].x = rc.right;
                    vertex[1].y = rc.bottom;
                    vertex[1].Red = selstep.effect_1 << 8;
                    vertex[1].Green = selstep.effect_2 << 8;
                    vertex[1].Blue = selstep.effect_3 << 8;
                    vertex[1].Alpha = 0xFFFF;

                    gRect.UpperLeft = 0;
                    gRect.LowerRight = 1;
                    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
                    breaks;
                }
            }
            DeleteObject(c1);
        }
        char* ky;
        CONTROL_TEXT txt;
        int idx;
        

        MapIterate(texts, ky, txt, idx, 
            if (!txt.enabled) continue;
            TextOut(hdc, txt.x, txt.y, txt.text, txt.len);
        );

        EndPaint(hwnd, &ps);
        DeleteDC(hdc);
    }
    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
