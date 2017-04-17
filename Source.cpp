#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "wininet.lib")

#include <windows.h>
#include <wininet.h>
#include <string>

TCHAR szClassName[] = TEXT("Window");

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
	WCHAR hdrs[] = L"Content-Type: application/x-www-form-urlencoded";
	const HINTERNET hInternet = InternetOpen(TEXT("WININET Test Program"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL)
	{
		goto END4;
	}
	const HINTERNET hHttpSession = InternetConnectW(hInternet, lpszServer, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (!hHttpSession)
	{
		goto END3;
	}
	const HINTERNET hHttpRequest = HttpOpenRequestW(hHttpSession, L"POST", lpszPath, NULL, 0, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
	if (!hHttpRequest)
	{
		goto END2;
	}
	DWORD dwTextLen = WideCharToMultiByte(CP_UTF8, 0, lpszData, -1, 0, 0, 0, 0);
	LPSTR lpszDataA = (LPSTR)GlobalAlloc(GPTR, dwTextLen);
	WideCharToMultiByte(CP_UTF8, 0, lpszData, -1, lpszDataA, dwTextLen, 0, 0);
	if (HttpSendRequestW(hHttpRequest, hdrs, lstrlenW(hdrs), lpszDataA, lstrlenA(lpszDataA)) == FALSE)
	{
		goto END1;
	}
	{
		LPBYTE lpszByte = (LPBYTE)GlobalAlloc(GPTR, 1);
		DWORD dwRead, dwSize = 0;
		static BYTE szBuf[1024 * 4];
		for (;;)
		{
			if (!InternetReadFile(hHttpRequest, szBuf, (DWORD)sizeof(szBuf), &dwRead) || !dwRead) break;
			LPBYTE lpTemp = (LPBYTE)GlobalReAlloc(lpszByte, (SIZE_T)(dwSize + dwRead), GMEM_MOVEABLE);
			if (lpTemp == NULL) break;
			lpszByte = lpTemp;
			CopyMemory(lpszByte + dwSize, szBuf, dwRead);
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
END1:
	GlobalFree(lpszDataA);
	InternetCloseHandle(hHttpRequest);
END2:
	InternetCloseHandle(hHttpSession);
END3:
	InternetCloseHandle(hInternet);
END4:
	return lpszReturn;
}

BOOL GetClientIDAndClientSecret(HWND hEditOutput, LPCWSTR lpszServer, LPWSTR lpszId, LPWSTR lpszSecret)
{
	BOOL bRetutnValue = FALSE;
	WCHAR szData[1024];
	lstrcpyW(szData, L"client_name=kenjinote&redirect_uris=urn:ietf:wg:oauth:2.0:oob&scopes=write&website=https://hack.jp");
	LPWSTR lpszReturn = Post(lpszServer, L"/api/v1/apps", szData);
	if (lpszReturn)
	{
		SetWindowTextW(hEditOutput, lpszReturn);
		bRetutnValue = GetValueFromJSON(lpszReturn, L"client_id", lpszId) & GetValueFromJSON(lpszReturn, L"client_secret", lpszSecret);
		GlobalFree(lpszReturn);
	}
	return bRetutnValue;
}

BOOL GetAccessToken(HWND hEditOutput, LPCWSTR lpszServer, LPCWSTR lpszId, LPCWSTR lpszSecret, LPCWSTR lpszUserName, LPCWSTR lpszPassword, LPWSTR lpszAccessToken)
{
	BOOL bRetutnValue = FALSE;
	WCHAR szData[1024];
	wsprintfW(szData, L"scope=write&client_id=%s&client_secret=%s&grant_type=password&username=%s&password=%s", lpszId, lpszSecret, lpszUserName, lpszPassword);
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hEdit1;
	static HWND hEdit2;
	static HWND hEdit3;
	static HWND hEdit4;
	static HWND hEdit6;
	static HWND hCombo;
	static HWND hButton;
	switch (msg)
	{
	case WM_CREATE:
		hEdit1 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("mastodon.social"), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hEdit2 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("user@example.com"), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hEdit3 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("password1234"), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_PASSWORD, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hEdit4 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("アプリケーションからの投稿テスト"), WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hEdit4, EM_LIMITTEXT, 500, 0);
		hEdit6 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), 0, WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hCombo = CreateWindow(TEXT("COMBOBOX"), 0, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("公開"));
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("非公開"));
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("フォロアーのみ閲覧"));
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("ダイレクトメッセージ"));
		SendMessage(hCombo, CB_SETCURSEL, 0, 0);
		hButton = CreateWindow(TEXT("BUTTON"), TEXT("投稿"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)IDOK, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		break;
	case WM_SIZE:
		MoveWindow(hEdit1, 10, 10, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(hEdit2, 10, 50, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(hEdit3, 10, 90, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(hEdit4, 10, 130, LOWORD(lParam) - 20, 96, TRUE);
		MoveWindow(hCombo, 10, 234, LOWORD(lParam) - 20, 256, TRUE);
		MoveWindow(hButton, 10, 274, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(hEdit6, 10, 314, LOWORD(lParam) - 20, HIWORD(lParam) - 324, TRUE);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			WCHAR szServer[256] = { 0 };
			GetWindowTextW(hEdit1, szServer, _countof(szServer));
			WCHAR szUserName[256] = { 0 };
			GetWindowTextW(hEdit2, szUserName, _countof(szUserName));
			WCHAR szPassword[256] = { 0 };
			GetWindowTextW(hEdit3, szPassword, _countof(szPassword));
			WCHAR szMessage[501] = { 0 };
			GetWindowTextW(hEdit4, szMessage, _countof(szMessage));
			const int nVisibility = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
			LPWSTR lpszVisibility = 0;
			switch (nVisibility)
			{
			case 0:
				lpszVisibility = L"public";
				break;
			case 1:
				lpszVisibility = L"unlisted";
				break;
			case 2:
				lpszVisibility = L"private";
				break;
			case 3:
				lpszVisibility = L"direct";
				break;
			}
			BOOL bSuccess = FALSE;
			WCHAR szClientID[65] = { 0 };
			WCHAR szSecret[65] = { 0 };
			bSuccess = GetClientIDAndClientSecret(hEdit6, szServer, szClientID, szSecret);
			if (!bSuccess) return 0;
			WCHAR szAccessToken[65] = { 0 };
			bSuccess = GetAccessToken(hEdit6, szServer, szClientID, szSecret, szUserName, szPassword, szAccessToken);
			if (!bSuccess) return 0;
			WCHAR szCreatedAt[32] = { 0 };
			bSuccess = Toot(hEdit6, szServer, szAccessToken, szMessage, lpszVisibility, szCreatedAt);
			if (!bSuccess) return 0;
			WCHAR szResult[1024];
			wsprintfW(szResult, L"投稿されました。\n投稿日時 = %s", szCreatedAt);
			MessageBoxW(hWnd, szResult, L"確認", 0);
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefDlgProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	MSG msg;
	WNDCLASS wndclass = { 0,WndProc,0,DLGWINDOWEXTRA,hInstance,0,LoadCursor(0,IDC_ARROW),0,0,szClassName };
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(szClassName, TEXT("Toot"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 512, 512, 0, 0, hInstance, 0);
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