#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std;

const DWORD MAGIC = 0x5354454D;
const int HEADER_SIZE = 8;

Gdiplus::Image* g_img = nullptr;
vector<BYTE> g_data;
int g_w = 0, g_h = 0;

ULONG_PTR gdiplusToken;

const wchar_t* ENCODE_CLASS = L"EncodeDialog";
const wchar_t* DECODE_CLASS = L"DecodeDialog";

enum {
    LOAD_IMAGE = 2001,
    WRITE_MESSAGE = 102,
    DECODE_MESSAGE = 104,
    SAVE_IMAGE = 105,
    BTN_OK = 1001,
    BTN_CANCEL = 1002,
    EDIT_MESSAGE = 1003
};

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EncodeDialog(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DecodeDialog(HWND, UINT, WPARAM, LPARAM);
void ShowModalDialog(HWND parent, const wchar_t* className, const wchar_t* title, int width, int height);

void SetLSB(BYTE& p, int b) { p = (p & 0xFE) | b; }
int GetLSB(BYTE p) { return p & 1; }

bool LoadImageFromFile(const wchar_t* path)
{
    if (g_img) delete g_img;
    g_img = new Gdiplus::Image(path);

    if (g_img->GetLastStatus() != Ok) {
        delete g_img;
        g_img = nullptr;
        return false;
    }

    g_w = g_img->GetWidth();
    g_h = g_img->GetHeight();

    Bitmap* bmp = new Bitmap(g_w, g_h, PixelFormat32bppARGB);
    Graphics* gfx = Graphics::FromImage(bmp);
    gfx->DrawImage(g_img, 0, 0, g_w, g_h);
    delete gfx;

    BitmapData bd;
    Rect rect(0, 0, g_w, g_h);
    bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bd);

    g_data.resize(g_w * g_h * 4);
    memcpy(g_data.data(), bd.Scan0, g_data.size());

    bmp->UnlockBits(&bd);
    delete bmp;
    return true;
}

bool EmbedMessage(const wstring& msg)
{
    if (g_data.empty()) return false;

    // Convertir en UTF-8
    int utf8Sz = WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1, 0, 0, 0, 0);
    if (utf8Sz <= 1) return false;
    vector<char> utf8(utf8Sz);
    WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1, utf8.data(), utf8Sz, 0, 0);
    DWORD len = utf8Sz - 1; // sans le NUL

    // Capacité en bits : 3 canaux par pixel (B,G,R), g_data a 4 octets/pixel
    long pixelCount = g_w * g_h;
    long capacityBits = pixelCount * 3; // 3 bits par pixel (un bit par canal)
    long neededBits = (HEADER_SIZE + (long)len) * 8L;

    if (neededBits > capacityBits) {
        MessageBox(0, L"Message trop long pour cette image !", L"Erreur", MB_OK);
        return false;
    }

    // Ecrire bit à bit : mapping k-th bit -> pixel = k / 3, channel = k % 3 (0=B,1=G,2=R)
    auto setDataBit = [&](long k, int bit) {
        long pixelIdx = k / 3;
        int channel = k % 3; // 0..2
        long byteIndex = pixelIdx * 4 + channel; // skip alpha (channel 3)
        SetLSB(g_data[byteIndex], bit);
        };

    long k = 0;
    // MAGIC (32 bits)
    for (int i = 0; i < 32; ++i) {
        int bit = (int)((MAGIC >> (31 - i)) & 1);
        setDataBit(k++, bit);
    }
    // LENGTH (32 bits)
    for (int i = 0; i < 32; ++i) {
        int bit = (int)((len >> (31 - i)) & 1);
        setDataBit(k++, bit);
    }
    // DATA
    for (DWORD i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)utf8[i];
        for (int b = 0; b < 8; ++b) {
            int bit = (c >> (7 - b)) & 1;
            setDataBit(k++, bit);
        }
    }

    return true;
}

wstring ExtractMessage()
{
    if (g_data.empty()) return L"";

    long pixelCount = g_w * g_h;
    long capacityBits = pixelCount * 3;

    auto getDataBit = [&](long k) -> int {
        long pixelIdx = k / 3;
        int channel = k % 3;
        long byteIndex = pixelIdx * 4 + channel;
        return GetLSB(g_data[byteIndex]);
        };

    long k = 0;
    // lire MAGIC
    DWORD magic = 0;
    for (int i = 0; i < 32; ++i) {
        if (k >= capacityBits) return L"[Aucun message detecte]";
        magic = (magic << 1) | (DWORD)getDataBit(k++);
    }
    if (magic != MAGIC) return L"[Aucun message detecte]";

    // lire LENGTH
    DWORD len = 0;
    for (int i = 0; i < 32; ++i) {
        if (k >= capacityBits) return L"[Erreur: longueur invalide]";
        len = (len << 1) | (DWORD)getDataBit(k++);
    }
    // sanity
    if (len == 0 || (long)len * 8L + k > capacityBits) return L"[Erreur: longueur invalide]";

    vector<char> utf8(len + 1);
    for (DWORD i = 0; i < len; ++i) {
        unsigned char c = 0;
        for (int b = 0; b < 8; ++b) {
            if (k >= capacityBits) return L"[Erreur lors de la lecture]";
            c = (unsigned char)((c << 1) | (unsigned char)getDataBit(k++));
        }
        utf8[i] = (char)c;
    }
    utf8[len] = 0;

    int wSz = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), -1, 0, 0);
    if (wSz <= 1) return L"";

    wstring res(wSz - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), -1, &res[0], wSz);
    return res;
}

bool SaveBMP(const wchar_t* path)
{
    if (g_data.empty()) return false;

    int rowSz = ((g_w * 3 + 3) / 4) * 4;
    int imgSz = rowSz * g_h;

    BITMAPFILEHEADER fh = {
        0x4D42,
        (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imgSz),
        0, 0,
        sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)
    };

    BITMAPINFOHEADER ih = {
        sizeof(BITMAPINFOHEADER),
        g_w, g_h, 1,
        24, BI_RGB,
        (DWORD)imgSz
    };

    HANDLE hf = CreateFile(
        path, GENERIC_WRITE, 0, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    );

    if (hf == INVALID_HANDLE_VALUE) return false;

    DWORD wr;
    WriteFile(hf, &fh, sizeof(fh), &wr, 0);
    WriteFile(hf, &ih, sizeof(ih), &wr, 0);

    vector<BYTE> row(rowSz);

    for (int y = g_h - 1; y >= 0; y--) {
        for (int x = 0; x < g_w; x++) {
            int i = (y * g_w + x) * 4;
            row[x * 3] = g_data[i];
            row[x * 3 + 1] = g_data[i + 1];
            row[x * 3 + 2] = g_data[i + 2];
        }
        WriteFile(hf, row.data(), rowSz, &wr, 0);
    }

    CloseHandle(hf);
    return true;
}

void ShowModalDialog(HWND parent, const wchar_t* className, const wchar_t* title, int width, int height)
{
    HWND dialog = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        className,
        title,
        WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        parent,
        NULL,
        (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE),
        NULL
    );

    RECT rp, rd;
    GetWindowRect(parent, &rp);
    GetWindowRect(dialog, &rd);

    SetWindowPos(
        dialog,
        HWND_TOP,
        rp.left + (rp.right - rp.left - (rd.right - rd.left)) / 2,
        rp.top + (rp.bottom - rp.top - (rd.bottom - rd.top)) / 2,
        0, 0,
        SWP_NOSIZE
    );

    EnableWindow(parent, FALSE);

    MSG msg;
    while (IsWindow(dialog)) {
        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetFocus(parent);
}

LRESULT CALLBACK EncodeDialog(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static HWND hEdit;

    switch (msg)
    {
    case WM_CREATE:
        CreateWindow(L"STATIC", L"Message a cacher :", WS_VISIBLE | WS_CHILD,
            20, 20, 360, 20, hwnd, 0, 0, 0);

        hEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            20, 50, 360, 25, hwnd, (HMENU)EDIT_MESSAGE, 0, 0
        );

        CreateWindow(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            220, 90, 80, 30, hwnd, (HMENU)BTN_OK, 0, 0);

        CreateWindow(L"BUTTON", L"Annuler", WS_VISIBLE | WS_CHILD,
            310, 90, 80, 30, hwnd, (HMENU)BTN_CANCEL, 0, 0);
        break;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case BTN_OK:
        {
            wchar_t buf[512];
            GetWindowText(hEdit, buf, 512);

            if (wcslen(buf) == 0)
                MessageBox(hwnd, L"Entrez un message.", L"Erreur", MB_OK);
            else if (EmbedMessage(buf)) {
                MessageBox(hwnd, L"Message cache !", L"Succes", MB_OK);
                DestroyWindow(hwnd);
            }
        }
        break;

        case BTN_CANCEL:
            DestroyWindow(hwnd);
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

LRESULT CALLBACK DecodeDialog(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindow(L"STATIC", L"Message trouve :", WS_VISIBLE | WS_CHILD,
            20, 20, 360, 20, hwnd, 0, 0, 0);

        HWND hEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY,
            20, 50, 360, 100, hwnd, 0, 0, 0);

        wstring msgDecoded = ExtractMessage();
        SetWindowText(hEdit, msgDecoded.c_str());

        CreateWindow(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            310, 160, 80, 30, hwnd, (HMENU)BTN_OK, 0, 0);
    }
    break;

    case WM_COMMAND:
        if (LOWORD(wp) == BTN_OK)
            DestroyWindow(hwnd);
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wp)) {

        case LOAD_IMAGE:
        {
            OPENFILENAME ofn = { sizeof(ofn) };
            wchar_t path[MAX_PATH] = L"";

            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Images\0*.bmp;*.png;*.jpg\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                if (LoadImageFromFile(path)) {
                    InvalidateRect(hwnd, NULL, TRUE);
                    MessageBox(hwnd, L"Image chargee!", L"OK", MB_OK);
                }
                else MessageBox(hwnd, L"Erreur chargement.", L"Erreur", MB_OK);
            }
            break;
        }

        case WRITE_MESSAGE:
            if (g_data.empty()) {
                MessageBox(hwnd, L"Chargez une image d'abord!", L"Erreur", MB_OK);
                break;
            }

            ShowModalDialog(hwnd, ENCODE_CLASS, L"Cacher un message", 420, 180);
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case DECODE_MESSAGE:
            if (g_data.empty()) {
                MessageBox(hwnd, L"Chargez une image d'abord!", L"Erreur", MB_OK);
                break;
            }

            ShowModalDialog(hwnd, DECODE_CLASS, L"Decoder un message", 420, 230);
            break;

        case SAVE_IMAGE:
        {
            if (g_data.empty()) {
                MessageBox(hwnd, L"Aucune image !", L"Erreur", MB_OK);
                break;
            }

            OPENFILENAME ofn = { sizeof(ofn) };
            wchar_t path[MAX_PATH] = L"image_cachee.bmp";

            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"BMP\0*.bmp\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = L"bmp";
            ofn.Flags = OFN_OVERWRITEPROMPT;

            if (GetSaveFileName(&ofn)) {
                if (SaveBMP(path))
                    MessageBox(hwnd, L"Image sauvegardee !", L"OK", MB_OK);
                else
                    MessageBox(hwnd, L"Erreur sauvegarde.", L"Erreur", MB_OK);
            }
            break;
        }
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (g_img) {
            Graphics gfx(hdc);
            RECT rc;
            GetClientRect(hwnd, &rc);
            gfx.DrawImage(g_img, 0, 0, rc.right, rc.bottom);
        }

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        if (g_img) delete g_img;
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    GdiplusStartupInput gsi;
    GdiplusStartup(&gdiplusToken, &gsi, NULL);

    WNDCLASS wc = { 0, WindowProc, 0, 0, hInst, 0,
                    0, (HBRUSH)(COLOR_WINDOW + 1), 0, L"MainWindow" };
    RegisterClass(&wc);

    WNDCLASS wc2 = { 0, EncodeDialog, 0, 0, hInst, 0,
                     LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
                     0, ENCODE_CLASS };
    RegisterClass(&wc2);

    WNDCLASS wc3 = { 0, DecodeDialog, 0, 0, hInst, 0,
                     LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
                     0, DECODE_CLASS };
    RegisterClass(&wc3);

    HWND hwnd = CreateWindowEx(
        0, L"MainWindow", L"Steganographie LSB",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        900, 600,
        0, 0, hInst, 0
    );

    HMENU hMenu = CreateMenu();
    HMENU hSub = CreateMenu();

    AppendMenu(hSub, MF_STRING, LOAD_IMAGE, L"Charger image");
    AppendMenu(hSub, MF_STRING, WRITE_MESSAGE, L"Cacher message");
    AppendMenu(hSub, MF_STRING, DECODE_MESSAGE, L"Decoder message");
    AppendMenu(hSub, MF_STRING, SAVE_IMAGE, L"Sauvegarder image");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSub, L"Options");
    SetMenu(hwnd, hMenu);

    ShowWindow(hwnd, nShow);

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return msg.wParam;
}
