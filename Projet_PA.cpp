#include <windows.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std;

Gdiplus::Image* g_pImage = nullptr;
HWND g_hEditMessage = nullptr;
HWND g_hBtnValidate = nullptr;
wstring g_userMessage;

// IDs
enum {
    LOAD_IMAGE = 2001,   // Nouveau : charger depuis fichier
    ID_MENU_WRITE = 102,
    ID_MENU_SHOW = 103,
    ID_MENU_DECODE = 104,
    ID_MENU_SAVE = 105,

    ID_BUTTON_LOAD_IMG = 1000,
    ID_BTN_VALIDATE = 1001,
    ID_EDIT_MESSAGE = 1002
};

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HMENU hMenuMain;
HMENU hMenuParam;

ULONG_PTR gdiplusToken; // nécessaire pour GDI+

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{

    // Initialisation GDI+
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Classe de fenêtre
    WNDCLASS wc = {};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MainWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) return 0;

    // Fenêtre principale
    HWND hwnd = CreateWindowEx(
        0, L"MainWindow", L"Projet Prog. Avancee", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    // Menus
    hMenuMain = CreateMenu();
    hMenuParam = CreateMenu();

    AppendMenu(hMenuParam, MF_STRING, LOAD_IMAGE, L"Afficher une image...");
    AppendMenu(hMenuParam, MF_STRING, ID_MENU_WRITE, L"Ecrire un Message");
    AppendMenu(hMenuParam, MF_STRING, ID_MENU_DECODE, L"Decoder le texte");
    AppendMenu(hMenuParam, MF_STRING, ID_MENU_SAVE, L"Sauvegarder l'image");
    AppendMenu(hMenuMain, MF_POPUP, (UINT_PTR)hMenuParam, L"Parametres");

    SetMenu(hwnd, hMenuMain);

    // Afficher
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // boucle de messages
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // arrêter GDI+
    GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}

// WndProc
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        switch (id)
        {

        case ID_BUTTON_LOAD_IMG:
            // Charge test.bmp en mémoire
            if (g_pImage) { delete g_pImage; g_pImage = nullptr; }
            g_pImage = new Image(L"test.bmp");
            MessageBox(hwnd, L"Image chargee ! Cliquez sur 'Afficher l'image'.", L"Info", MB_OK);
            break;


        case LOAD_IMAGE: // menu → choisir fichier
        {
            OPENFILENAME ofn;
            wchar_t filePath[MAX_PATH] = L"";

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Images (*.bmp)\0*.bmp;\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                if (g_pImage) { delete g_pImage; g_pImage = nullptr; }

                g_pImage = new Gdiplus::Image(filePath);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case ID_MENU_WRITE:
        {
            if (!g_hEditMessage)
            {
                g_hEditMessage = CreateWindowEx(
                    0, L"EDIT", L"",
                    WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                    50, 100, 300, 25,
                    hwnd, (HMENU)ID_EDIT_MESSAGE,
                    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                    NULL
                );

                g_hBtnValidate = CreateWindowEx(
                    0, L"BUTTON", L"Valider",
                    WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                    360, 100, 80, 25,
                    hwnd, (HMENU)ID_BTN_VALIDATE,
                    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                    NULL
                );
            }
            break;
        }

        case ID_BTN_VALIDATE:
        {
            wchar_t buffer[512];
            GetWindowText(g_hEditMessage, buffer, 512);
            g_userMessage = buffer;

            MessageBox(hwnd, g_userMessage.c_str(), L"Message reçu", MB_OK);

            DestroyWindow(g_hEditMessage);
            DestroyWindow(g_hBtnValidate);
            g_hEditMessage = nullptr;
            g_hBtnValidate = nullptr;
            break;
        }

        case ID_MENU_SHOW:
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        Graphics gfx(hdc);

        if (g_pImage) {
            RECT rc;
            GetClientRect(hwnd, &rc);  // récupère la taille de la fenêtre
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;


            gfx.DrawImage(g_pImage, 0, 0, width, height);
        }

        EndPaint(hwnd, &ps);
    }
    break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}