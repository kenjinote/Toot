#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "wininet")

#include <windows.h>
#include <wininet.h>
#include <string>
#include "resource.h"

BOOL GetValueFromJSON(LPCWSTR lpszJson, LPCWSTR lpszKey, LPWSTR lpszValue)
{
	std::wstring json(lpszJson);
	for (size_t posStart = json.find(lpszKey); posStart != std::wstring::npos;)
	{
		posStart += lstrlenW(lpszKey);
		posStart = json.find(L'\"', posStart);
		if (posStart == std::wstring::npos) return FALSE;
		posStart += 1;
		posStart = json.find(L':', posStart);
		if (posStart == std::wstring::npos) return FALSE;
		posStart += 1;
		posStart = json.find(L'\"', posStart);
		if (posStart == std::wstring::npos) return FALSE;
		posStart += 1;
		size_t posEnd = json.find(L'\"', posStart);
		if (posEnd == std::wstring::npos) return FALSE;
		std::wstring value(json, posStart, posEnd - posStart);
		lstrcpyW(lpszValue, value.c_str());
		return TRUE;
	}
	return FALSE;
}

LPWSTR Post(LPCWSTR lpszServer, LPCWSTR lpszPath, LPCWSTR lpszData)
{
	LPWSTR lpszReturn = 0;
	LPCWSTR hdrs = L"Content-Type: application/x-www-form-urlencoded";
	const HINTERNET hInternet = InternetOpen(TEXT("WININET Toot Program"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL)
	{
		goto END1;
	}
	const HINTERNET hHttpSession = InternetConnectW(hInternet, lpszServer, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (!hHttpSession)
	{
		goto END2;
	}
	const HINTERNET hHttpRequest = HttpOpenRequestW(hHttpSession, L"POST", lpszPath, NULL, 0, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
	if (!hHttpRequest)
	{
		goto END3;
	}
	DWORD dwTextLen = WideCharToMultiByte(CP_UTF8, 0, lpszData, -1, 0, 0, 0, 0);
	LPSTR lpszDataA = (LPSTR)GlobalAlloc(GPTR, dwTextLen);
	WideCharToMultiByte(CP_UTF8, 0, lpszData, -1, lpszDataA, dwTextLen, 0, 0);
	if (HttpSendRequestW(hHttpRequest, hdrs, lstrlenW(hdrs), lpszDataA, lstrlenA(lpszDataA)) == FALSE)
	{
		goto END4;
	}
	{
		LPBYTE lpszByte = (LPBYTE)GlobalAlloc(GPTR, 1);
		DWORD dwRead, dwSize = 0;
		static BYTE szBuffer[1024 * 4];
		for (;;)
		{
			if (!InternetReadFile(hHttpRequest, szBuffer, (DWORD)sizeof(szBuffer), &dwRead) || !dwRead) break;
			LPBYTE lpTemp = (LPBYTE)GlobalReAlloc(lpszByte, (SIZE_T)(dwSize + dwRead), GMEM_MOVEABLE);
			if (lpTemp == NULL) break;
			lpszByte = lpTemp;
			CopyMemory(lpszByte + dwSize, szBuffer, dwRead);
			dwSize += dwRead;
		}
		if (lpszByte[0])
		{
			dwTextLen = MultiByteToWideChar(CP_UTF8, 0, (LPSTR)lpszByte, -1, 0, 0);
			lpszReturn = (LPWSTR)GlobalAlloc(GPTR, dwTextLen * sizeof(WCHAR));
			MultiByteToWideChar(CP_UTF8, 0, (LPSTR)lpszByte, -1, lpszReturn, dwTextLen);
		}
		GlobalFree(lpszByte);
	}
END4:
	GlobalFree(lpszDataA);
	InternetCloseHandle(hHttpRequest);
END3:
	InternetCloseHandle(hHttpSession);
END2:
	InternetCloseHandle(hInternet);
END1:
	return lpszReturn;
}

BOOL GetClientIDAndClientSecret(HWND hEditOutput, LPCWSTR lpszServer, LPWSTR lpszID, LPWSTR lpszSecret)
{
	BOOL bRetutnValue = FALSE;
	WCHAR szData[1024];
	lstrcpyW(szData, L"client_name=kenjinote&redirect_uris=urn:ietf:wg:oauth:2.0:oob&scopes=write&website=https://hack.jp");
	LPWSTR lpszReturn = Post(lpszServer, L"/api/v1/apps", szData);
	if (lpszReturn)
	{
		SetWindowTextW(hEditOutput, lpszReturn);
		bRetutnValue = GetValueFromJSON(lpszReturn, L"client_id", lpszID) & GetValueFromJSON(lpszReturn, L"client_secret", lpszSecret);
		GlobalFree(lpszReturn);
	}
	return bRetutnValue;
}

BOOL GetAccessToken(HWND hEditOutput, LPCWSTR lpszServer, LPCWSTR lpszID, LPCWSTR lpszSecret, LPCWSTR lpszUserName, LPCWSTR lpszPassword, LPWSTR lpszAccessToken)
{
	BOOL bRetutnValue = FALSE;
	WCHAR szData[1024];
	wsprintfW(szData, L"scope=write&client_id=%s&client_secret=%s&grant_type=password&username=%s&password=%s", lpszID, lpszSecret, lpszUserName, lpszPassword);
	LPWSTR lpszReturn = Post(lpszServer, L"/oauth/token", szData);
	if (lpszReturn)
	{
		SetWindowTextW(hEditOutput, lpszReturn);
		bRetutnValue = GetValueFromJSON(lpszReturn, L"access_token", lpszAccessToken);
		GlobalFree(lpszReturn);
	}
	return bRetutnValue;
}

BOOL Toot(HWND hEditOutput, LPCWSTR lpszServer, LPCWSTR lpszAccessToken, LPCWSTR lpszMessage, LPCWSTR lpszVisibility, LPWSTR lpszCreatedAt)
{
	BOOL bRetutnValue = FALSE;
	WCHAR szData[1024];
	wsprintfW(szData, L"access_token=%s&status=%s&visibility=%s", lpszAccessToken, lpszMessage, lpszVisibility);
	LPWSTR lpszReturn = Post(lpszServer, L"/api/v1/statuses", szData);
	if (lpszReturn)
	{
		SetWindowTextW(hEditOutput, lpszReturn);
		bRetutnValue = GetValueFromJSON(lpszReturn, L"created_at", lpszCreatedAt);
		GlobalFree(lpszReturn);
	}
	return bRetutnValue;
}

class EditBox
{
	WNDPROC EditWndProc;
	LPTSTR m_lpszPlaceholder;
	static LRESULT CALLBACK EditProc1(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		EditBox* _this = (EditBox*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (_this)
		{
			if (msg == WM_PAINT && !GetWindowTextLength(hWnd))
			{
				PAINTSTRUCT ps;
				const HDC hdc = BeginPaint(hWnd, &ps);
				SetTextColor(hdc, RGB(180, 180, 180));
				PatBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom, WHITENESS);
				const HFONT hOldFont = (HFONT)SelectObject(hdc, (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0));
				TextOut(hdc, 2, 0, _this->m_lpszPlaceholder, lstrlen(_this->m_lpszPlaceholder));
				SelectObject(hdc, hOldFont);
				EndPaint(hWnd, &ps);
				return 0;
			}
			return CallWindowProc(_this->EditWndProc, hWnd, msg, wParam, lParam);
		}
		return 0;
	}
public:
	HWND m_hWnd;
	EditBox(LPCTSTR lpszDefaultText, DWORD dwStyle, int x, int y, int width, int height, HWND hParent, HMENU hMenu, LPCTSTR lpszPlaceholder)
		: m_hWnd(0)
		, EditWndProc(0)
		, m_lpszPlaceholder(0)
	{
		if (lpszPlaceholder && !m_lpszPlaceholder)
		{
			m_lpszPlaceholder = (LPTSTR)GlobalAlloc(0, sizeof(TCHAR) * (lstrlen(lpszPlaceholder) + 1));
			lstrcpy(m_lpszPlaceholder, lpszPlaceholder);
		}
		m_hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), lpszDefaultText, dwStyle, x, y, width, height, hParent, hMenu, GetModuleHandle(0), 0);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
		EditWndProc = (WNDPROC)SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)EditProc1);
	}
	~EditBox() { DestroyWindow(m_hWnd); GlobalFree(m_lpszPlaceholder); }
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static EditBox *pEdit1, *pEdit2, *pEdit3, *pEdit4;
	static HWND hEdit5, hCombo, hButton;
	static HFONT hFont;
	switch (msg)
	{
	case WM_CREATE:
		hFont = CreateFont(24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Yu Gothic UI"));
		pEdit1 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, TEXT("サーバー名 (mstdn.jp など)"));
		SendMessage(pEdit1->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit2 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, TEXT("メールアドレス"));
		SendMessage(pEdit2->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit3 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 0, 0, 0, 0, hWnd, 0, TEXT("パスワード"));
		SendMessage(pEdit3->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit4 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN, 0, 0, 0, 0, hWnd, 0, TEXT("今何してる？"));
		SendMessage(pEdit4->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(pEdit4->m_hWnd, EM_LIMITTEXT, 500, 0);
		hCombo = CreateWindow(TEXT("COMBOBOX"), 0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("公開"));
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("非公開"));
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("フォロアーのみ閲覧"));
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("ダイレクトメッセージ"));
		SendMessage(hCombo, CB_SETCURSEL, 0, 0);
		hButton = CreateWindow(TEXT("BUTTON"), TEXT("投 稿"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)IDOK, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hButton, WM_SETFONT, (WPARAM)hFont, 0);
		hEdit5 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), 0, WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hEdit5, WM_SETFONT, (WPARAM)hFont, 0);
		break;
	case WM_SIZE:
		MoveWindow(pEdit1->m_hWnd, 10, 10, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit2->m_hWnd, 10, 50, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit3->m_hWnd, 10, 90, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit4->m_hWnd, 10, 130, LOWORD(lParam) - 20, 96, TRUE);
		MoveWindow(hCombo, 10, 234, LOWORD(lParam) - 20, 256, TRUE);
		MoveWindow(hButton, 10, 274, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(hEdit5, 10, 314, LOWORD(lParam) - 20, HIWORD(lParam) - 324, TRUE);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			WCHAR szServer[256] = { 0 };
			GetWindowTextW(pEdit1->m_hWnd, szServer, _countof(szServer));
			WCHAR szUserName[256] = { 0 };
			GetWindowTextW(pEdit2->m_hWnd, szUserName, _countof(szUserName));
			WCHAR szPassword[256] = { 0 };
			GetWindowTextW(pEdit3->m_hWnd, szPassword, _countof(szPassword));
			WCHAR szMessage[501] = { 0 };
			GetWindowTextW(pEdit4->m_hWnd, szMessage, _countof(szMessage));
			const int nVisibility = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
			LPWSTR lpszVisibility = 0;
			switch (nVisibility)
			{
			case 0:
				lpszVisibility = L"public";
				break;
			case 1:
				lpszVisibility = L"private";
				break;
			case 2:
				lpszVisibility = L"unlisted";
				break;
			case 3:
				lpszVisibility = L"direct";
				break;
			}
			BOOL bSuccess = FALSE;
			WCHAR szClientID[65] = { 0 };
			WCHAR szSecret[65] = { 0 };
			bSuccess = GetClientIDAndClientSecret(hEdit5, szServer, szClientID, szSecret);
			if (!bSuccess) return 0;
			WCHAR szAccessToken[65] = { 0 };
			bSuccess = GetAccessToken(hEdit5, szServer, szClientID, szSecret, szUserName, szPassword, szAccessToken);
			if (!bSuccess) return 0;
			WCHAR szCreatedAt[32] = { 0 };
			bSuccess = Toot(hEdit5, szServer, szAccessToken, szMessage, lpszVisibility, szCreatedAt);
			if (!bSuccess) return 0;
			WCHAR szResult[1024];
			wsprintfW(szResult, L"投稿されました。\n投稿日時 = %s", szCreatedAt);
			MessageBoxW(hWnd, szResult, L"確認", 0);
			SetWindowText(pEdit4->m_hWnd, 0);
			SetFocus(pEdit4->m_hWnd);
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		delete pEdit1;
		delete pEdit2;
		delete pEdit3;
		delete pEdit4;
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	default:
		return DefDlgProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	const TCHAR szClassName[] = TEXT("TootWindow");
	MSG msg;
	const WNDCLASS wndclass = { 0,WndProc,0,DLGWINDOWEXTRA,hInstance,LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)),LoadCursor(0,IDC_ARROW),0,0,szClassName };
	RegisterClass(&wndclass);
	const HWND hWnd = CreateWindow(szClassName, TEXT("Toot"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 512, 512, 0, 0, hInstance, 0);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		if (!IsDialogMessage(hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}