// mergetxt.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "mergetxt.h"


#pragma comment(lib, "comctl32.lib")
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "uthash/uthash.h"  // 第三方哈希库，下载地址 https://troydhanson.github.io/uthash/

#define ID_LISTBOX 101
#define ID_TEXTBOX 102
#define ID_BUTTON_ADD 103
#define ID_BUTTON_PROCESS 104
#define ID_PROGRESS_BAR 105
#define ID_PROGRESS_TEXT 106

typedef struct {
    char line[1024];
    UT_hash_handle hh;
} LineEntry;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AddFileToList(HWND hListBox, HWND hTextBox);
void ProcessFiles(HWND hListBox, HWND hTextBox, HWND hProgressBar, HWND hProgressText);
void SaveLinesToFile(LineEntry* lines);
int CountLinesInFile(const wchar_t* filePath);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MainWindowClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"File Processor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 500,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

static int is_runing = 0;
struct _hwnds{

    HWND hListBox;
    HWND hTextBox;
    HWND hProgressBar;
    HWND hProgressText;
};

DWORD WINAPI thread_process(void* params)
{

    struct _hwnds* phwnds = params;
    ProcessFiles(phwnds->hListBox, phwnds->hTextBox, phwnds->hProgressBar, phwnds->hProgressText);

    free(phwnds);
    is_runing = 0;
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hListBox, hTextBox, hButtonAdd, hButtonProcess, hProgressBar, hProgressText;

    switch (uMsg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        hListBox = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            WC_LISTBOX,
            NULL,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
            20, 20, 440, 200,
            hwnd,
            (HMENU)ID_LISTBOX,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        hTextBox = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            WC_EDIT,
            L"Prefix to delete",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            20, 230, 200, 25,
            hwnd,
            (HMENU)ID_TEXTBOX,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        hButtonAdd = CreateWindow(
            WC_BUTTON,
            L"Add File",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            230, 230, 100, 25,
            hwnd,
            (HMENU)ID_BUTTON_ADD,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        hButtonProcess = CreateWindow(
            WC_BUTTON,
            L"Process",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            340, 230, 100, 25,
            hwnd,
            (HMENU)ID_BUTTON_PROCESS,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        hProgressBar = CreateWindowEx(
            0,
            PROGRESS_CLASS,
            NULL,
            WS_CHILD | WS_VISIBLE,
            20, 270, 440, 25,
            hwnd,
            (HMENU)ID_PROGRESS_BAR,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        hProgressText = CreateWindowEx(
            0,
            WC_STATIC,
            NULL,
            WS_CHILD | WS_VISIBLE,
            20, 310, 440, 25,
            hwnd,
            (HMENU)ID_PROGRESS_TEXT,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        break;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_BUTTON_ADD) {
            if (is_runing == 0) {
                AddFileToList(hListBox, hTextBox);
            }
            
        }
        else if (LOWORD(wParam) == ID_BUTTON_PROCESS) {

            if (is_runing == 0) {

                struct _hwnds* phwnds = malloc(sizeof(struct _hwnds));
                phwnds->hListBox = hListBox;
                phwnds->hTextBox = hTextBox;
                phwnds->hProgressBar = hProgressBar;
                phwnds->hProgressText = hProgressText;

                is_runing = 1;
                CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_process, phwnds, 1, 0);
            }

            
        }
        break;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 创建双缓冲所需的设备上下文和位图
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

        // 在内存中进行绘制操作
        // 例如：绘制背景、文本、图形等

        // 将内存中的内容绘制到屏幕上
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
            memDC, 0, 0, SRCCOPY);

        // 清理资源
        SelectObject(memDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);

        break;
    }
    
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

void AddFileToList(HWND hListBox, HWND hTextBox) {
    OPENFILENAME ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hListBox;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
    ofn.lpstrFilter = L"Text Files\0*.TXT\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)ofn.lpstrFile);
    }
}

void ProcessFiles(HWND hListBox, HWND hTextBox, HWND hProgressBar, HWND hProgressText) {
    int count = SendMessage(hListBox, LB_GETCOUNT, 0, 0);
    if (count == LB_ERR || count == 0) {
        MessageBox(NULL, L"No files to process!", L"Error", MB_ICONERROR);
        return;
    }

    wchar_t textValue[256];
    GetWindowText(hTextBox, textValue, 256);
    char skipValue[256];
    wcstombs(skipValue, textValue, 256);
    size_t skipValueLen = strlen(skipValue);

    LineEntry* lines = NULL;
    int totalLines = 0;
    for (int i = 0; i < count; i++) {
        wchar_t filePath[260];
        SendMessage(hListBox, LB_GETTEXT, i, (LPARAM)filePath);
        totalLines += CountLinesInFile(filePath);
    }

    SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(hProgressBar, PBM_SETSTEP, (WPARAM)1, 0);

    int processedLines = 0;

    for (int i = 0; i < count; i++) {
        wchar_t filePath[260];
        SendMessage(hListBox, LB_GETTEXT, i, (LPARAM)filePath);

        FILE* file = _wfopen(filePath, L"r");
        if (!file) continue;

        char line[1024];
        while (fgets(line, sizeof(line), file)) {
            processedLines++;

            char progressText[256];
            int progressPercent = (processedLines * 100) / totalLines;
            sprintf(progressText, "Progress: %d%% (%d/%d lines)", progressPercent, processedLines, totalLines);
            SetWindowTextA(hProgressText, progressText);

            SendMessage(hProgressBar, PBM_SETPOS, (WPARAM)progressPercent, 0);


            if (memcmp(line, skipValue, skipValueLen) != 0) continue;

            LineEntry* entry;
            HASH_FIND_STR(lines, line, entry);
            if (!entry) {
                entry = (LineEntry*)malloc(sizeof(LineEntry));
                strcpy(entry->line, line);
                HASH_ADD_STR(lines, line, entry);
            }
        }
        fclose(file);
    }

    SaveLinesToFile(lines);

    LineEntry* current_entry, * tmp;
    HASH_ITER(hh, lines, current_entry, tmp) {
        HASH_DEL(lines, current_entry);
        free(current_entry);
    }

    MessageBox(NULL, L"Processing complete!", L"Information", MB_ICONINFORMATION);
}

void SaveLinesToFile(LineEntry* lines) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    char fileName[256];
    strftime(fileName, sizeof(fileName), "%Y%m%d%H%M%S.txt", t);

    FILE* file = fopen(fileName, "w");
    if (!file) {
        MessageBox(NULL, L"Could not create output file!", L"Error", MB_ICONERROR);
        return;
    }

    LineEntry* entry;
    for (entry = lines; entry != NULL; entry = entry->hh.next) {
        fputs(entry->line, file);
    }

    fclose(file);
}

int CountLinesInFile(const wchar_t* filePath) {
    FILE* file = _wfopen(filePath, L"r");
    if (!file) return 0;

    int lines = 0;
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        lines++;
    }

    fclose(file);
    return lines;
}
