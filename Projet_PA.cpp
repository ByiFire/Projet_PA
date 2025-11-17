#include <windows.h>

// Déclaration de la fonction de traitement des messages
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
HMENU Param;
HMENU Menu;
// Point d'entrée de l'application Windows
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1️ Définir la classe de fenêtre
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;             // Fonction de traitement des messages
    wc.hInstance = hInstance;             // Instance de l'application
    wc.lpszClassName = L"MainWindow";     // Nom unique de la classe


    wc.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); // Couleur de fond par défaut (blanche)

    // 2️ Enregistrer la classe auprès du système
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Erreur : Enregistrement de la classe échoué.", L"Erreur", MB_ICONERROR);
        return 0;
    }

    // 3️ Créer une fenêtre basée sur la classe enregistrée
    HWND hwnd = CreateWindowEx(
        0, L"MainWindow", L"H&D Texts", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, L"Erreur : Création de la fenêtre échouée.", L"Erreur", MB_ICONERROR);
        return 0;
    }

    Menu = CreateMenu();
    Param = CreateMenu();




    AppendMenu(Param, MF_STRING, 1, L"Charger une image");
    AppendMenu(Param, MF_STRING, 2, L"Ecrire un message");
    AppendMenu(Param, MF_STRING, 3, L"Afficher l'image");
    AppendMenu(Param, MF_STRING, 4, L"Decoder le texte");
    AppendMenu(Param, MF_STRING, 5, L"Sauvegarder l'image");
    AppendMenu(Menu, MF_POPUP, (UINT_PTR(Param)), L"Parametres");



    SetMenu(hwnd, Menu);

    // 4️ Afficher la fenêtre à l'écran
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 5️ Boucle principale de messages
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); // Traduit les entrées clavier (ex: touches accentuées)
        DispatchMessage(&msg);  // Envoie le message à WndProc
    }

    return (int)msg.wParam;
    
}

static int windowWidth;
static int windowHeight;
// EXERCICE 6
static BOOL  g_dragging = FALSE;
static POINT g_startMouse = {};   // position souris (écran) au clic
static RECT  g_startWnd = {};   // rect fenêtre (écran) au clic
// EXERCICE 7
static int g_colorIndex = 0;
static HBRUSH g_bgBrush = NULL;

// Fonction de traitement des messages (callback)
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);  // Quand l’utilisateur ferme la fenêtre
        break;

    case WM_DESTROY:
        PostQuitMessage(0);   // Termine la boucle principale
        break;
        
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {          // ID du bouton
            MessageBox(hwnd, L"Az op", L"Info", MB_OK);
        }
        break;
    

    case WM_KEYDOWN: {
        // EXERCICE 4
        if (wParam == VK_ESCAPE) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        //wchar_t keyName[64] = L"";
        //GetKeyNameText((LONG)lParam, keyName, 64);
        //wchar_t title[128];
        //wsprintf(title, L"Derniere touche: %s", keyName);
        //SetWindowText(hwnd, title);
        break;
    }
    

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam); // Traitement par défaut
    }
    return 0;
}