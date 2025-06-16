#include <string>
#include <sstream>

#define USE_PC // 定義を切り替えることでPC版とX68000版を切り替え可能

#ifdef USE_PC
#include <windows.h>
// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif // USE_PC

#ifndef USE_PC
#include <stdio.h>
#include <stdint.h>
#include <iostream>

// IOCSコールをアセンブラで呼び出す
void msinit()
{
    __asm__ volatile (
        "moveq  #0x70, %%d0\n\t" // _MS_INIT
        "trap   #15\n\t"
        :
        :                        // 入力なし
        : "d0"   // 使用するレジスタ
        );
}

void mscuron()
{
    __asm__ volatile (
        "moveq  #0x71, %%d0\n\t" // _MS_CURON
        "trap   #15\n\t"
        :
        :                        // 入力なし
        : "d0"   // 使用するレジスタ
        );
}

void msstat(int* px, int* py, int* pbl, int* pbr)
{
    uint32_t stat;

    __asm__ volatile (
        "moveq  #0x74, %%d0\n\t" // _MS_GETDT
        "trap   #15\n\t"
        "move.l %%d0, %0\n\t"
        : "=r"(stat)
        :                        // 入力なし
        : "d0"   // 使用するレジスタ
        );

    *px = (stat >> 24) & 0xff;
    *py = (stat >> 16) & 0xff;
    *pbl = (stat >> 8) & 0xff;
    *pbr = (stat >> 0) & 0xff;
}

void mspos(int* px, int* py)
{
    uint32_t pos;

    __asm__ volatile (
        "moveq  #0x75, %%d0\n\t" // _MS_CURGT
        "trap   #15\n\t"
        "move.l %%d0, %0\n\t"
        : "=r"(pos)
        :                        // 入力なし
        : "d0"   // 使用するレジスタ
        );

    *px = (pos >> 16) & 0xffff;
    *py = (pos >> 0) & 0xffff;
}

void keysns(int* pkey)
{
    uint32_t key;

    __asm__ volatile (
        "moveq  #1, %%d0\n\t" // _B_KEYSNS
        "trap   #15\n\t"
        "move.l %%d0, %0\n\t"
        : "=r"(key)
        :                        // 入力なし
        : "d0"   // 使用するレジスタ
        );

    *pkey = key;
}

// ソフトウェアキーボード消去
void skeymod_off()
{
    __asm__ volatile (
        "moveq  #0x7d, %%d0\n\t" // _SKEY_MOD
        "moveq  #0, %%d1\n\t"
        "trap   #15\n\t"
        :
        :                        // 入力なし
        : "d0", "d1"   // 使用するレジスタ
        );
}

void mouse_init()
{
    msinit();
    mscuron();
    skeymod_off();
}
#endif

typedef struct {
    int x;
    int y;
    bool bl;
    bool br;
} MouseState;

#ifdef USE_PC
void get_mouse_state(MouseState* state)
{
    POINT pt; // マウス座標を格納する構造体
    if (GetCursorPos(&pt)) {
        state->x = pt.x;
        state->y = pt.y;
    }
    else {
        state->x = -1;
        state->y = -1;
    }

    // マウスボタンの状態を取得
    state->bl = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    state->br = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}
#else
// マウスの状態を取得する関数
void get_mouse_state(MouseState* state)
{
    int x, y, x2, y2, bl, br;

    mspos(&x, &y);
    msstat(&x2, &y2, &bl, &br);

    state->x = x;
    state->y = y;
    state->bl = bl != 0;
    state->br = br != 0;
}
#endif

#ifdef USE_PC
//HWND ghwnd = NULL; // ウィンドウハンドルをグローバルに定義
bool window_init(HWND& hwnd) {
    // コンソールアプリだけどウィンドウを作る！
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const wchar_t CLASS_NAME[] = L"MyMouseWindow";

    // ウィンドウクラスの登録
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // ウィンドウを作成
    hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"マウスボタンテスト",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBox(NULL, L"ウィンドウの作成に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);

	return true;
}

bool window_or_mouse_init(HWND& hwnd) {
    // ウィンドウを初期化
    return window_init(hwnd);
}
#else
typedef int HWND; // X68000ではウィンドウは不要なのでダミー定義
bool window_or_mouse_init(HWND& hwnd) {
    mouse_init();
	return true; // X68000ではウィンドウは不要なので常に成功
}
#endif

int main() {
    MouseState state;

    HWND hwnd;
    if (!window_or_mouse_init(hwnd)) {
        // ウィンドウの初期化に失敗した場合は終了
        return 1;
    }

    while (true) {
#ifdef USE_PC
        MSG msg;
        // メッセージがあれば処理
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#endif // USE_PC

        get_mouse_state(&state);

        std::stringstream ss;
        ss << "Position: (" << state.x << ", " << state.y << ") ";
        ss << "Buttons: Left: " << (state.bl ? "ON" : "OFF")
            << " | Right: " << (state.br ? "ON" : "OFF");

        printf("\r%s  ", ss.str().c_str());
        fflush(stdout);

#ifdef USE_PC
        // CPU負荷を減らすために少し待つ
        Sleep(50);
#else
        // キー入力をチェックして終了
        int key = 0;
        keysns(&key);

        if (key)
            break;
#endif // USE_PC
    }

#ifndef USE_PC
    // キー読み捨て
    getchar();
#endif // USE_PC

    return 0;
}
