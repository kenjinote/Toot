#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "wininet")
#pragma comment(lib, "gdiplus")
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "dwmapi")

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <wininet.h>
#include <string>
#include <list>
#include <vector>
#include "resource.h"

Gdiplus::Bitmap* LoadBitmapFromResource(int nID, LPCWSTR lpszType)
{
	Gdiplus::Bitmap* pBitmap = 0;
	const HINSTANCE hInstance = GetModuleHandle(0);
	const HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCE(nID), lpszType);
	if (!hResource)
		return 0;
	const DWORD dwImageSize = SizeofResource(hInstance, hResource);
	if (!dwImageSize)
		return 0;
	const void* pResourceData = LockResource(LoadResource(hInstance, hResource));
	if (!pResourceData)
		return 0;
	const HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, dwImageSize);
	if (hBuffer) {
		void* pBuffer = GlobalLock(hBuffer);
		if (pBuffer) {
			CopyMemory(pBuffer, pResourceData, dwImageSize);
			IStream* pStream = NULL;
			if (CreateStreamOnHGlobal(hBuffer, TRUE, &pStream) == S_OK) {
				pBitmap = Gdiplus::Bitmap::FromStream(pStream);
				if (pBitmap) {
					if (pBitmap->GetLastStatus() != Gdiplus::Ok) {
						delete pBitmap;
						pBitmap = NULL;
					}
				}
				pStream->Release();
			}
			GlobalUnlock(hBuffer);
		}
	}
	return pBitmap;
}

class Mastodon {
	LPWSTR m_lpszServer;
	WCHAR m_szClientID[65];
	WCHAR m_szSecret[65];
	static std::wstring Trim(const std::wstring& string, LPCWSTR trimCharacterList = L" \"\t\v\r\n") {
		std::wstring result;
		std::wstring::size_type left = string.find_first_not_of(trimCharacterList);
		if (left != std::wstring::npos) {
			std::wstring::size_type right = string.find_last_not_of(trimCharacterList);
			result = string.substr(left, right - left + 1);
		}
		return result;
	}
	static BOOL GetValueFromJSON(LPCWSTR lpszJson, LPCWSTR lpszKey, LPWSTR lpszValue) {
		std::wstring json(lpszJson);
		std::wstring key(lpszKey);
		key = L"\"" + key + L"\"";
		size_t posStart = json.find(key);
		if (posStart == std::wstring::npos) return FALSE;
		posStart += key.length();
		posStart = json.find(L':', posStart);
		if (posStart == std::wstring::npos) return FALSE;
		++posStart;
		size_t posEnd = json.find(L',', posStart);
		if (posEnd == std::wstring::npos) {
			posEnd = json.find(L'}', posStart);
			if (posEnd == std::wstring::npos) return FALSE;
		}
		std::wstring value(json, posStart, posEnd - posStart);
		value = Trim(value);
		lstrcpyW(lpszValue, value.c_str());
		return TRUE;
	}
public:
	WCHAR m_szAccessToken[65];
	Mastodon() : m_lpszServer(0) {
		m_szAccessToken[0] = 0;
		m_szClientID[0] = 0;
		m_szSecret[0] = 0;
	};
	~Mastodon() {
		GlobalFree(m_lpszServer);
	}
	void SetServer(LPCWSTR lpszServer) {
		GlobalFree(m_lpszServer);
		m_lpszServer = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR) * (lstrlenW(lpszServer) + 1));
		lstrcpyW(m_lpszServer, lpszServer);
	}
	LPWSTR Post(LPCWSTR lpszPath, LPCWSTR lpszHeader, LPBYTE lpbyData, int nSize) const {
		LPWSTR lpszReturn = 0;
		if (!m_lpszServer && !m_lpszServer[0]) goto END1;
		const HINTERNET hInternet = InternetOpenW(L"WinInet Toot Program", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
		if (!hInternet) goto END1;
		const HINTERNET hHttpSession = InternetConnectW(hInternet, m_lpszServer, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
		if (!hHttpSession) goto END2;
		const HINTERNET hHttpRequest = HttpOpenRequestW(hHttpSession, L"POST", lpszPath, NULL, 0, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
		if (!hHttpRequest) goto END3;
		if (HttpSendRequestW(hHttpRequest, lpszHeader, lstrlenW(lpszHeader), lpbyData, nSize) == FALSE) goto END4;
		{
			LPBYTE lpszByte = (LPBYTE)GlobalAlloc(GPTR, 1);
			DWORD dwRead, dwSize = 0;
			static BYTE szBuffer[1024 * 4];
			for (;;) {
				if (!InternetReadFile(hHttpRequest, szBuffer, (DWORD)sizeof(szBuffer), &dwRead) || !dwRead) break;
				LPBYTE lpTemp = (LPBYTE)GlobalReAlloc(lpszByte, (SIZE_T)(dwSize + dwRead + 1), GMEM_MOVEABLE);
				if (lpTemp == NULL) break;
				lpszByte = lpTemp;
				CopyMemory(lpszByte + dwSize, szBuffer, dwRead);
				dwSize += dwRead;
			}
			lpszByte[dwSize] = 0;
			if (lpszByte[0]) {
				DWORD dwTextLen = MultiByteToWideChar(CP_UTF8, 0, (LPSTR)lpszByte, -1, 0, 0);
				lpszReturn = (LPWSTR)GlobalAlloc(GPTR, dwTextLen * sizeof(WCHAR));
				MultiByteToWideChar(CP_UTF8, 0, (LPSTR)lpszByte, -1, lpszReturn, dwTextLen);
			}
			GlobalFree(lpszByte);
		}
	END4:
		InternetCloseHandle(hHttpRequest);
	END3:
		InternetCloseHandle(hHttpSession);
	END2:
		InternetCloseHandle(hInternet);
	END1:
		return lpszReturn;
	}
	BOOL GetClientIDAndClientSecret() {
		BOOL bReturnValue = FALSE;
		CHAR szData[128];
		lstrcpyA(szData, "client_name=TootApp&redirect_uris=urn:ietf:wg:oauth:2.0:oob&scopes=write");
		LPWSTR lpszReturn = Post(L"/api/v1/apps", L"Content-Type: application/x-www-form-urlencoded", (LPBYTE)szData, lstrlenA(szData));
		if (lpszReturn) {
			bReturnValue = GetValueFromJSON(lpszReturn, L"client_id", m_szClientID) & GetValueFromJSON(lpszReturn, L"client_secret", m_szSecret);
			GlobalFree(lpszReturn);
		}
		return bReturnValue;
	}
	BOOL GetAccessToken(LPCWSTR lpszUserName, LPCWSTR lpszPassword) {
		BOOL bReturnValue = FALSE;
		if (!m_szClientID[0]) return bReturnValue;
		if (!m_szSecret[0]) return bReturnValue;
		WCHAR szData[512];
		wsprintfW(szData, L"scope=write&client_id=%s&client_secret=%s&grant_type=password&username=%s&password=%s", m_szClientID, m_szSecret, lpszUserName, lpszPassword);
		DWORD dwTextLen = WideCharToMultiByte(CP_UTF8, 0, szData, -1, 0, 0, 0, 0);
		LPSTR lpszDataA = (LPSTR)GlobalAlloc(GPTR, dwTextLen);
		WideCharToMultiByte(CP_UTF8, 0, szData, -1, lpszDataA, dwTextLen, 0, 0);
		LPWSTR lpszReturn = Post(L"/oauth/token", L"Content-Type: application/x-www-form-urlencoded", (LPBYTE)lpszDataA, dwTextLen - 1);
		GlobalFree(lpszDataA);
		if (lpszReturn) {
			bReturnValue = GetValueFromJSON(lpszReturn, L"access_token", m_szAccessToken);
			GlobalFree(lpszReturn);
		}
		return bReturnValue;
	}
	BOOL Toot(LPCWSTR lpszMessage, LPCWSTR lpszVisibility, LPWSTR lpszCreatedAt, const std::vector<int> &mediaIds, BOOL bCheckNsfw) const {
		BOOL bReturnValue = FALSE;
		if (!m_szAccessToken[0]) return bReturnValue;
		WCHAR szData[1024];
		wsprintfW(szData, L"access_token=%s&status=%s&visibility=%s", m_szAccessToken, lpszMessage, lpszVisibility);
		if (mediaIds.size()) {
			for (auto id : mediaIds) {
				WCHAR szID[32];
				wsprintfW(szID, L"&media_ids[]=%d", id);
				lstrcatW(szData, szID);
			}
			if (bCheckNsfw)
				lstrcatW(szData, L"&sensitive=true");
		}
		DWORD dwTextLen = WideCharToMultiByte(CP_UTF8, 0, szData, -1, 0, 0, 0, 0);
		LPSTR lpszDataA = (LPSTR)GlobalAlloc(GPTR, dwTextLen);
		WideCharToMultiByte(CP_UTF8, 0, szData, -1, lpszDataA, dwTextLen, 0, 0);
		LPWSTR lpszReturn = Post(L"/api/v1/statuses", L"Content-Type: application/x-www-form-urlencoded", (LPBYTE)lpszDataA, dwTextLen);
		GlobalFree(lpszDataA);
		if (lpszReturn) {
			bReturnValue = GetValueFromJSON(lpszReturn, L"created_at", lpszCreatedAt);
			GlobalFree(lpszReturn);
		}
		return bReturnValue;
	}
	VOID MakeBoundary(LPWSTR lpszBoundary) const {
		lstrcpyW(lpszBoundary, L"----Boundary");
		lstrcatW(lpszBoundary, L"kQcRxxn2b2BGpt9a");
	}
	BOOL MediaUpload(LPCWSTR lpszMediaType, LPBYTE lpbyImageData, int nDataLength, LPWSTR lpszTextURL, int *pMediaID) const {
		BOOL bReturnValue = FALSE;
		if (!m_szAccessToken[0]) return bReturnValue;
		WCHAR szBoundary[32];
		MakeBoundary(szBoundary);
		WCHAR szHeader[256];
		wsprintfW(szHeader, L"Authorization: Bearer %s\r\nContent-Type: multipart/form-data; boundary=%s", m_szAccessToken, szBoundary);
		CHAR szMediaTypeA[16];
		WideCharToMultiByte(CP_ACP, 0, lpszMediaType, -1, szMediaTypeA, _countof(szMediaTypeA), 0, 0);
		CHAR szFileNameA[16];
		lstrcpyA(szFileNameA, szMediaTypeA);
		for (int i = 0; i < lstrlenA(szFileNameA); ++i) {
			if (szFileNameA[i] == '/') {
				szFileNameA[i] = '.';
				break;
			}
		}
		CHAR szBoundaryA[32];
		WideCharToMultiByte(CP_ACP, 0, szBoundary, -1, szBoundaryA, _countof(szBoundaryA), 0, 0);
		CHAR szDataBeforeA[256];
		int nSizeBefore = wsprintfA(szDataBeforeA, "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n", szBoundaryA, szFileNameA, szMediaTypeA);
		CHAR szDataAfterA[64];
		int nSizeAfter = wsprintfA(szDataAfterA, "--%s--\r\n", szBoundaryA);
		int nTotalSize = nSizeBefore + nDataLength + nSizeAfter;
		LPBYTE lpbyData = (LPBYTE)GlobalAlloc(0, nTotalSize);
		CopyMemory(lpbyData, szDataBeforeA, nSizeBefore);
		CopyMemory(lpbyData + nSizeBefore, lpbyImageData, nDataLength);
		CopyMemory(lpbyData + nSizeBefore + nDataLength, szDataAfterA, nSizeAfter);
		LPWSTR lpszReturn = Post(L"/api/v1/media", szHeader, lpbyData, nTotalSize);
		GlobalFree(lpbyData);
		if (lpszReturn) {
			bReturnValue = GetValueFromJSON(lpszReturn, L"url", lpszTextURL) && !wcsstr(lpszTextURL, L"/missing.");
			if (bReturnValue) {
				WCHAR szMediaID[16];
				bReturnValue = GetValueFromJSON(lpszReturn, L"id", szMediaID);
				if (bReturnValue) {
					*pMediaID = _wtol(szMediaID);
				}
			}
			GlobalFree(lpszReturn);
		}
		return bReturnValue;
	}
};

class EditBox {
	WNDPROC fnEditWndProc;
	LPWSTR m_lpszPlaceholder;
	int m_nLimit;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		EditBox* _this = (EditBox*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (_this) {
			if (msg == WM_PAINT) {
				const LRESULT lResult = CallWindowProc(_this->fnEditWndProc, hWnd, msg, wParam, lParam);
				const int nTextLength = GetWindowTextLengthW(hWnd);
				if ((_this->m_lpszPlaceholder && !nTextLength) || _this->m_nLimit) {
					const HDC hdc = GetDC(hWnd);
					const COLORREF OldTextColor = SetTextColor(hdc, RGB(180, 180, 180));
					const COLORREF OldBkColor = SetBkColor(hdc, RGB(255, 255, 255));
					const HFONT hOldFont = (HFONT)SelectObject(hdc, (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0));
					const int nLeft = LOWORD(SendMessageW(hWnd, EM_GETMARGINS, 0, 0));
					if (_this->m_lpszPlaceholder && !nTextLength) {
						TextOutW(hdc, nLeft + 4, 2, _this->m_lpszPlaceholder, lstrlenW(_this->m_lpszPlaceholder));
					}
					if (_this->m_nLimit) {
						RECT rect;
						GetClientRect(hWnd, &rect);
						--rect.right;
						--rect.bottom;
						WCHAR szText[16];
						wsprintfW(szText, L"%5d", _this->m_nLimit - nTextLength);
						DrawTextW(hdc, szText, -1, &rect, DT_RIGHT | DT_SINGLELINE | DT_BOTTOM);
					}
					SelectObject(hdc, hOldFont);
					SetBkColor(hdc, OldBkColor);
					SetTextColor(hdc, OldTextColor);
					ReleaseDC(hWnd, hdc);
				}
				return lResult;
			}
			else if (msg == WM_CHAR && wParam == 1) {
				SendMessageW(hWnd, EM_SETSEL, 0, -1);
				return 0;
			}
			return CallWindowProc(_this->fnEditWndProc, hWnd, msg, wParam, lParam);
		}
		return 0;
	}
public:
	HWND m_hWnd;
	EditBox(LPCWSTR lpszDefaultText, DWORD dwStyle, int x, int y, int width, int height, HWND hParent, HMENU hMenu, LPCWSTR lpszPlaceholder)
		: m_hWnd(0), m_nLimit(0), fnEditWndProc(0), m_lpszPlaceholder(0) {
		if (lpszPlaceholder && !m_lpszPlaceholder) {
			m_lpszPlaceholder = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR) * (lstrlenW(lpszPlaceholder) + 1));
			lstrcpyW(m_lpszPlaceholder, lpszPlaceholder);
		}
		m_hWnd = CreateWindowW(L"EDIT", lpszDefaultText, dwStyle, x, y, width, height, hParent, hMenu, GetModuleHandle(0), 0);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
		fnEditWndProc = (WNDPROC)SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
	}
	~EditBox() { DestroyWindow(m_hWnd); GlobalFree(m_lpszPlaceholder); }
	void SetLimit(int nLimit) {
		SendMessageW(m_hWnd, EM_LIMITTEXT, nLimit, 0);
		m_nLimit = nLimit;
	}
};

class BitmapEx : public Gdiplus::Bitmap {
public:
	LPBYTE m_lpByte;
	DWORD m_nSize;
	BitmapEx(IN HBITMAP hbm)
		: Gdiplus::Bitmap::Bitmap(hbm, 0)
		, m_lpByte(0), m_nSize(0) {
		Gdiplus::Status OldlastResult = GetLastStatus();
		if (OldlastResult == Gdiplus::Ok) {
			GUID guid;
			if (GetRawFormat(&guid) == Gdiplus::Ok) {
			}
		}
		else {
			lastResult = OldlastResult;
		}
	}
	BitmapEx(const WCHAR *filename)
		: Gdiplus::Bitmap::Bitmap(filename)
		, m_lpByte(0), m_nSize(0) {
		if (GetLastStatus() == Gdiplus::Ok) {
			GUID guid;
			if (GetRawFormat(&guid) == Gdiplus::Ok) {
				if (guid == Gdiplus::ImageFormatGIF) {
					UINT count = GetFrameDimensionsCount();
					GUID* pDimensionIDs = new GUID[count];
					GetFrameDimensionsList(pDimensionIDs, count);
					int nFrameCount = GetFrameCount(&pDimensionIDs[0]);
					delete[]pDimensionIDs;
					if (nFrameCount > 1) {
						HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
						if (hFile != INVALID_HANDLE_VALUE) {
							DWORD dwReadSize;
							m_nSize = GetFileSize(hFile, 0);
							m_lpByte = (LPBYTE)GlobalAlloc(0, m_nSize);
							ReadFile(hFile, m_lpByte, m_nSize, &dwReadSize, 0);
							CloseHandle(hFile);
						}
					}
				}
			}
		}
		else {
			lastResult = Gdiplus::UnknownImageFormat;
		}
	}
	virtual ~BitmapEx() {
		GlobalFree(m_lpByte);
		m_lpByte = 0;
	}
};

BitmapEx* WindowCapture(HWND hWnd)
{
	BitmapEx * pBitmap = 0;
	RECT rect1;
	GetWindowRect(hWnd, &rect1);
	RECT rect2;
	if (DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect2, sizeof(rect2)) != S_OK) rect2 = rect1;
	HDC hdc = GetDC(0);
	HDC hMem = CreateCompatibleDC(hdc);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdc, rect2.right - rect2.left, rect2.bottom - rect2.top);
	if (hBitmap) {
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMem, hBitmap);
		SetForegroundWindow(hWnd);
		InvalidateRect(hWnd, 0, 1);
		UpdateWindow(hWnd);
		BitBlt(hMem, 0, 0, rect2.right - rect2.left, rect2.bottom - rect2.top, hdc, rect2.left, rect2.top, SRCCOPY);
		pBitmap = new BitmapEx(hBitmap);
		SelectObject(hMem, hOldBitmap);
		DeleteObject(hBitmap);
	}
	DeleteDC(hMem);
	ReleaseDC(0, hdc);
	return pBitmap;
}

BitmapEx* ScreenCapture(LPRECT lpRect)
{
	BitmapEx * pBitmap = 0;
	HDC hdc = GetDC(0);
	HDC hMem = CreateCompatibleDC(hdc);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdc, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top);
	if (hBitmap) {
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMem, hBitmap);
		BitBlt(hMem, 0, 0, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top, hdc, lpRect->left, lpRect->top, SRCCOPY);
		pBitmap = new BitmapEx(hBitmap);
		SelectObject(hMem, hOldBitmap);
		DeleteObject(hBitmap);
	}
	DeleteDC(hMem);
	ReleaseDC(0, hdc);
	return pBitmap;
}

LRESULT CALLBACK LayerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hParentWnd;
	static BOOL bDrag;
	static BOOL bDown;
	static POINT posStart;
	static RECT OldRect;
	switch (msg) {
	case WM_CREATE:
		hParentWnd = (HWND)((LPCREATESTRUCT)lParam)->lpCreateParams;
		break;
	case WM_KEYDOWN:
	case WM_RBUTTONDOWN:
		SendMessage(hWnd, WM_CLOSE, 0, 0);
		break;
	case WM_LBUTTONDOWN:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		POINT point = { xPos, yPos };
		ClientToScreen(hWnd, &point);
		posStart = point;
		SetCapture(hWnd);
	}
	break;
	case WM_MOUSEMOVE:
		if (GetCapture() == hWnd)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			POINT point = { xPos, yPos };
			ClientToScreen(hWnd, &point);
			if (!bDrag) {
				if (abs(xPos - posStart.x) > GetSystemMetrics(SM_CXDRAG) && abs(yPos - posStart.y) > GetSystemMetrics(SM_CYDRAG)) {
					bDrag = TRUE;
				}
			}
			else {
				HDC hdc = GetDC(hWnd);
				RECT rect = { min(point.x, posStart.x), min(point.y, posStart.y), max(point.x, posStart.x), max(point.y, posStart.y) };
				OffsetRect(&rect, -GetSystemMetrics(SM_XVIRTUALSCREEN), -GetSystemMetrics(SM_YVIRTUALSCREEN));
				HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
				HRGN hRgn1 = CreateRectRgn(OldRect.left, OldRect.top, OldRect.right, OldRect.bottom);
				HRGN hRgn2 = CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
				CombineRgn(hRgn1, hRgn1, hRgn2, RGN_DIFF);
				FillRgn(hdc, hRgn1, (HBRUSH)GetStockObject(BLACK_BRUSH));
				FillRect(hdc, &rect, hBrush);
				OldRect = rect;
				DeleteObject(hBrush);
				DeleteObject(hRgn1);
				DeleteObject(hRgn2);
				ReleaseDC(hWnd, hdc);
			}
		}
		break;
	case WM_LBUTTONUP:
		if (GetCapture() == hWnd) {
			ReleaseCapture();
			Gdiplus::Bitmap * pBitmap = 0;
			if (bDrag) {
				bDrag = FALSE;
				int xPos = GET_X_LPARAM(lParam);
				int yPos = GET_Y_LPARAM(lParam);
				POINT point = { xPos, yPos };
				ClientToScreen(hWnd, &point);
				RECT rect = { min(point.x, posStart.x), min(point.y, posStart.y), max(point.x, posStart.x), max(point.y, posStart.y) };
				ShowWindow(hWnd, SW_HIDE);
				pBitmap = ScreenCapture(&rect);
			}
			else {
				ShowWindow(hWnd, SW_HIDE);
				HWND hTargetWnd = WindowFromPoint(posStart);
				hTargetWnd = GetAncestor(hTargetWnd, GA_ROOT);
				if (hTargetWnd) {
					pBitmap = WindowCapture(hTargetWnd);
				}
			}
			SendMessage(hParentWnd, WM_APP, 0, (LPARAM)pBitmap);
		}
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

class ImageListPanel {
	BOOL m_bDrag;
	int m_nDragIndex;
	int m_nSplitPrevIndex;
	int m_nSplitPrevPosY;
	int m_nMargin;
	int m_nImageMaxCount;
	HFONT m_hFont;
	std::list<BitmapEx*> m_listBitmap;
	WNDPROC fnWndProc;
	Gdiplus::Bitmap *m_pCameraIcon;
	BOOL MoveImage(int nIndexFrom, int nIndexTo) {
		if (nIndexFrom < 0) nIndexFrom = 0;
		if (nIndexTo < 0) nIndexTo = 0;
		if (nIndexFrom == nIndexTo) return FALSE;
		std::list<BitmapEx*>::iterator itFrom = m_listBitmap.begin();
		std::list<BitmapEx*>::iterator itTo = m_listBitmap.begin();
		std::advance(itFrom, nIndexFrom);
		std::advance(itTo, nIndexTo);
		m_listBitmap.splice(itTo, m_listBitmap, itFrom);
		return TRUE;
	}
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (msg == WM_NCCREATE) {
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			return TRUE;
		}
		ImageListPanel* _this = (ImageListPanel*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (_this) {
			switch (msg) {
			case WM_DROPFILES:
			{
				HDROP hDrop = (HDROP)wParam;
				WCHAR szFileName[MAX_PATH];
				UINT iFile, nFiles;
				nFiles = DragQueryFile((HDROP)hDrop, 0xFFFFFFFF, NULL, 0);
				BOOL bUpdate = FALSE;
				for (iFile = 0; iFile < nFiles; ++iFile) {
					if ((int)_this->m_listBitmap.size() >= _this->m_nImageMaxCount) break;
					DragQueryFileW(hDrop, iFile, szFileName, _countof(szFileName));
					BitmapEx* pBitmap = new BitmapEx(szFileName);
					if (pBitmap) {
						if (pBitmap->GetLastStatus() == Gdiplus::Ok) {
							_this->m_listBitmap.push_back(pBitmap);
							bUpdate = TRUE;
						}
						else {
							delete pBitmap;
						}
					}
				}
				DragFinish(hDrop);
				if (bUpdate)
					InvalidateRect(hWnd, 0, 1);
			}
			return 0;
			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				{
					RECT rect;
					GetClientRect(hWnd, &rect);
					INT nTop = _this->m_nMargin;
					Gdiplus::Graphics g(hdc);
					int nWidth = rect.right - 2 * _this->m_nMargin;
					Gdiplus::StringFormat f;
					f.SetAlignment(Gdiplus::StringAlignmentCenter);
					f.SetLineAlignment(Gdiplus::StringAlignmentCenter);
					if (_this->m_listBitmap.size() == 0) {
						Gdiplus::Font font(hdc, _this->m_hFont);
						Gdiplus::RectF rectf((Gdiplus::REAL)0, (Gdiplus::REAL)0, (Gdiplus::REAL)rect.right, (Gdiplus::REAL)rect.bottom);
						g.DrawString(L"画像をドロップ\r\n\r\nまたは\r\n\r\nクリックして画像を選択", -1, &font, rectf, &f, &Gdiplus::SolidBrush(Gdiplus::Color::MakeARGB(128, 0, 0, 0)));
					}
					else {
						Gdiplus::Font font(&Gdiplus::FontFamily(L"Marlett"), 11, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
						for (auto bitmap : _this->m_listBitmap) {
							int nHeight = bitmap->GetHeight() * nWidth / bitmap->GetWidth();
							g.DrawImage(bitmap, _this->m_nMargin, nTop, nWidth, nHeight);
							Gdiplus::RectF rectf((Gdiplus::REAL)(nWidth + _this->m_nMargin - 16), (Gdiplus::REAL)nTop, (Gdiplus::REAL)16, (Gdiplus::REAL)16);
							g.FillRectangle(&Gdiplus::SolidBrush(Gdiplus::Color::MakeARGB(192, 255, 255, 255)), rectf);
							g.DrawString(L"r", 1, &font, rectf, &f, &Gdiplus::SolidBrush(Gdiplus::Color::MakeARGB(192, 0, 0, 0)));
							nTop += nHeight + _this->m_nMargin;
						}
					}
					int nCameraIconWidth = _this->m_pCameraIcon->GetWidth();
					int nCameraIconHeigth = _this->m_pCameraIcon->GetHeight();
					g.DrawImage(_this->m_pCameraIcon, rect.right - nCameraIconWidth - 2, rect.bottom - nCameraIconHeigth - 2, nCameraIconWidth, nCameraIconHeigth);
				}
				EndPaint(hWnd, &ps);
			}
			return 0;
			case WM_APP:
			{
				BitmapEx* pBitmap = (BitmapEx*)lParam;
				BOOL bPushed = FALSE;
				if ((int)_this->m_listBitmap.size() < _this->m_nImageMaxCount) {
					if (pBitmap) {
						_this->m_listBitmap.push_back(pBitmap);
						InvalidateRect(hWnd, 0, 1);
						bPushed = TRUE;
					}
				}
				if (!bPushed)
					delete pBitmap;
				SetForegroundWindow(hWnd);
			}
			break;
			case WM_LBUTTONDOWN:
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				POINT point = { LOWORD(lParam), HIWORD(lParam) };
				int nCameraIconWidth = _this->m_pCameraIcon->GetWidth();
				int nCameraIconHeigth = _this->m_pCameraIcon->GetHeight();
				RECT rectCameraIcon = { rect.right - nCameraIconWidth - 2, rect.bottom - nCameraIconHeigth - 2, rect.right, rect.bottom };
				if (PtInRect(&rectCameraIcon, point)) {
					HWND hLayerWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST, L"LayerWindow", 0, WS_POPUP, 0, 0, 0, 0, 0, 0, GetModuleHandle(0), (LPVOID)hWnd);
					SetLayeredWindowAttributes(hLayerWnd, RGB(255, 0, 0), 64, LWA_ALPHA | LWA_COLORKEY);
					SetWindowPos(hLayerWnd, HWND_TOPMOST, GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN), SWP_NOSENDCHANGING);
					ShowWindow(hLayerWnd, SW_NORMAL);
					UpdateWindow(hLayerWnd);
					return 0;
				}
				INT nTop = _this->m_nMargin;
				int nWidth = rect.right - 2 * _this->m_nMargin;
				for (auto it = _this->m_listBitmap.begin(); it != _this->m_listBitmap.end(); ++it) {
					int nHeight = (*it)->GetHeight() * nWidth / (*it)->GetWidth();
					RECT rectCloseButton = { nWidth + _this->m_nMargin - 16, nTop, nWidth + _this->m_nMargin, nTop + 16 };
					if (PtInRect(&rectCloseButton, point)) {
						delete *it;
						*it = 0;
						_this->m_listBitmap.erase(it);
						InvalidateRect(hWnd, 0, 1);
						return 0;
					}
					nTop += nHeight + _this->m_nMargin;
				}
				nTop = _this->m_nMargin;
				int nIndex = 0;
				for (auto it = _this->m_listBitmap.begin(); it != _this->m_listBitmap.end(); ++it) {
					int nHeight = (*it)->GetHeight() * nWidth / (*it)->GetWidth();
					RECT rectImage = { _this->m_nMargin, nTop, _this->m_nMargin + nWidth, nTop + nHeight };
					if (PtInRect(&rectImage, point)) {
						_this->m_bDrag = TRUE;
						SetCapture(hWnd);
						_this->m_nDragIndex = nIndex;
						return 0;
					}
					nTop += nHeight + _this->m_nMargin;
					++nIndex;
				}
				if ((int)_this->m_listBitmap.size() < _this->m_nImageMaxCount) {
					WCHAR szFileName[MAX_PATH] = { 0 };
					OPENFILENAMEW of = { sizeof(OPENFILENAME) };
					WCHAR szMyDocumentFolder[MAX_PATH];
					SHGetFolderPathW(NULL, CSIDL_MYPICTURES, NULL, NULL, szMyDocumentFolder);//
					PathAddBackslashW(szMyDocumentFolder);
					of.hwndOwner = hWnd;
					of.lpstrFilter = L"画像ファイル\0*.png;*.gif;*.jpg;*.jpeg;*.bmp;*.tif;*.ico;*.emf;*.wmf;\0すべてのファイル(*.*)\0*.*\0\0";
					of.lpstrFile = szFileName;
					of.nMaxFile = MAX_PATH;
					of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
					of.lpstrTitle = L"画像ファイルを開く";
					of.lpstrInitialDir = szMyDocumentFolder;
					if (GetOpenFileNameW(&of)) {
						BitmapEx* pBitmap = new BitmapEx(szFileName);
						if (pBitmap) {
							if (pBitmap->GetLastStatus() == Gdiplus::Ok) {
								_this->m_listBitmap.push_back(pBitmap);
								InvalidateRect(hWnd, 0, 1);
							}
							else {
								delete pBitmap;
							}
						}
					}
				}
			}
			return 0;
			case WM_MOUSEMOVE:
				if (_this->m_bDrag) {
					RECT rect;
					GetClientRect(hWnd, &rect);
					INT nCursorY = HIWORD(lParam);
					INT nTop = 0;
					int nWidth = rect.right - 2 * _this->m_nMargin;
					int nIndex = 0;
					for (auto it = _this->m_listBitmap.begin(); it != _this->m_listBitmap.end(); ++it) {
						int nHeight = (*it)->GetHeight() * nWidth / (*it)->GetWidth();
						RECT rectImage = { 0, nTop, rect.right, nTop + nHeight + _this->m_nMargin };
						if (nCursorY >= nTop && (nIndex + 1 == _this->m_listBitmap.size() || nCursorY < nTop + nHeight + _this->m_nMargin)) {
							int nCurrentIndex;
							int nCurrentPosY;
							if (nCursorY < nTop + nHeight / 2 + _this->m_nMargin) {
								nCurrentIndex = nIndex;
								nCurrentPosY = nTop;
							}
							else {
								nCurrentIndex = nIndex + 1;
								nCurrentPosY = nTop + nHeight + _this->m_nMargin;
							}
							if (nCurrentIndex != _this->m_nSplitPrevIndex) {
								HDC hdc = GetDC(hWnd);
								if (_this->m_nSplitPrevIndex != -1)
									PatBlt(hdc, 0, _this->m_nSplitPrevPosY, rect.right, _this->m_nMargin, PATINVERT);
								PatBlt(hdc, 0, nCurrentPosY, rect.right, _this->m_nMargin, PATINVERT);
								ReleaseDC(hWnd, hdc);
								_this->m_nSplitPrevIndex = nCurrentIndex;
								_this->m_nSplitPrevPosY = nCurrentPosY;
							}
							return 0;
						}
						nTop += nHeight + _this->m_nMargin;
						++nIndex;
					}
				}
				return 0;
			case WM_LBUTTONUP:
				if (_this->m_bDrag) {
					ReleaseCapture();
					_this->m_bDrag = FALSE;
					if (_this->m_nSplitPrevIndex != -1) {
						RECT rect;
						GetClientRect(hWnd, &rect);
						HDC hdc = GetDC(hWnd);
						PatBlt(hdc, 0, _this->m_nSplitPrevPosY, rect.right, _this->m_nMargin, PATINVERT);
						ReleaseDC(hWnd, hdc);
						if (_this->MoveImage(_this->m_nDragIndex, _this->m_nSplitPrevIndex)) {
							InvalidateRect(hWnd, 0, 1);
						}
						_this->m_nSplitPrevIndex = -1;
					}
				}
				return 0;
			}
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	void RemoveAllImage() {
		for (auto &bitmap : m_listBitmap) {
			delete bitmap;
			bitmap = 0;
		}
		m_listBitmap.clear();
	}
public:
	HWND m_hWnd;
	ImageListPanel(int nImageMaxCount, DWORD dwStyle, int x, int y, int width, int height, HWND hParent, HFONT hFont)
		: m_nImageMaxCount(nImageMaxCount)
		, m_hWnd(0)
		, fnWndProc(0)
		, m_nMargin(4)
		, m_bDrag(0)
		, m_nSplitPrevIndex(-1)
		, m_nSplitPrevPosY(0)
		, m_hFont(hFont)
		, m_pCameraIcon(0) {
		m_pCameraIcon = LoadBitmapFromResource(IDB_PNG1, L"PNG");
		WNDCLASSW wndclass1 = { 0,LayerWndProc,0,0,GetModuleHandle(0),0,LoadCursor(0,IDC_CROSS),(HBRUSH)GetStockObject(BLACK_BRUSH),0,L"LayerWindow" };
		RegisterClassW(&wndclass1);
		WNDCLASSW wndclass2 = { CS_HREDRAW | CS_VREDRAW,WndProc,0,0,GetModuleHandle(0),0,LoadCursor(0,IDC_ARROW),(HBRUSH)(COLOR_WINDOW + 1),0,__FUNCTIONW__ };
		RegisterClassW(&wndclass2);
		m_hWnd = CreateWindowW(__FUNCTIONW__, 0, dwStyle, x, y, width, height, hParent, 0, GetModuleHandle(0), this);
	}
	~ImageListPanel() {
		RemoveAllImage();
		delete m_pCameraIcon;
	}
	int GetImageCount() { return (int)m_listBitmap.size(); }
	BitmapEx * GetImage(int nIndex) {
		std::list<BitmapEx*>::iterator it = m_listBitmap.begin();
		std::advance(it, nIndex);
		return *it;
	}
	void ResetContent() {
		RemoveAllImage();
		InvalidateRect(m_hWnd, 0, 1);
	}
};

HHOOK g_hHook;
LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HCBT_ACTIVATE) {
		UnhookWindowsHookEx(g_hHook);
		RECT rectMessageBox, rectParentWnd;
		GetWindowRect((HWND)wParam, &rectMessageBox);
		GetWindowRect(GetParent((HWND)wParam), &rectParentWnd);
		SetWindowPos((HWND)wParam, 0, (rectParentWnd.right + rectParentWnd.left - rectMessageBox.right + rectMessageBox.left) >> 1, (rectParentWnd.bottom + rectParentWnd.top - rectMessageBox.bottom + rectMessageBox.top) >> 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	return 0;
}

BOOL GetEncoderClsid(LPCWSTR format, CLSID* pClsid) {
	UINT  num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return FALSE;
	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(GlobalAlloc(0, size));
	if (pImageCodecInfo == NULL) return FALSE;
	GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT i = 0; i < num; ++i) {
		if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0) {
			*pClsid = pImageCodecInfo[i].Clsid;
			GlobalFree(pImageCodecInfo);
			return TRUE;
		}
	}
	GlobalFree(pImageCodecInfo);
	return FALSE;
}

LPWSTR GetText(HWND hWnd) {
	const int nSize = GetWindowTextLengthW(hWnd);
	LPWSTR lpszText = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR)*(nSize + 1));
	GetWindowTextW(hWnd, lpszText, nSize + 1);
	return lpszText;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static EditBox *pEdit1, *pEdit2, *pEdit3, *pEdit4;
	static ImageListPanel *pImageListPanel;
	static HWND hCombo, hButton, hCheckNsfw;
	static HFONT hFont;
	static BOOL bModified;
	static Mastodon* pMastodon;
	switch (msg) {
	case WM_CREATE:
		hFont = CreateFontW(22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"Yu Gothic UI");
		pEdit1 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, L"サーバー名");
		SendMessageW(pEdit1->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit2 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, L"メールアドレス");
		SendMessageW(pEdit2->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit3 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 0, 0, 0, 0, hWnd, 0, L"パスワード");
		SendMessageW(pEdit3->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit4 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN, 0, 0, 0, 0, hWnd, 0, L"今何してる？");
		SendMessageW(pEdit4->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit4->SetLimit(500);
		pImageListPanel = new ImageListPanel(4, WS_VISIBLE | WS_CHILD | WS_BORDER, 0, 0, 0, 0, hWnd, hFont);
		hCheckNsfw = CreateWindowW(L"BUTTON", L"NSFWチェック", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessageW(hCheckNsfw, WM_SETFONT, (WPARAM)hFont, 0);
		hCombo = CreateWindowW(L"COMBOBOX", 0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessageW(hCombo, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(hCombo, CB_SETITEMDATA, SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"全体に公開"), (LPARAM)L"public");
		SendMessageW(hCombo, CB_SETITEMDATA, SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"自分のタイムラインに公開（公開タイムラインには表示しない）"), (LPARAM)L"unlisted");
		SendMessageW(hCombo, CB_SETITEMDATA, SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"自分のフォロワーに公開"), (LPARAM)L"private");
		SendMessageW(hCombo, CB_SETITEMDATA, SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"非公開・ダイレクトメッセージ（自分と@ユーザーのみ閲覧可）"), (LPARAM)L"direct");
		SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
		hButton = CreateWindowW(L"BUTTON", L"トゥート! (Ctrl + Enter)", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1000, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessageW(hButton, WM_SETFONT, (WPARAM)hFont, 0);
		pMastodon = new Mastodon;
		DragAcceptFiles(hWnd, TRUE);
		break;
	case WM_DROPFILES:
		if (pImageListPanel) {
			SendMessageW(pImageListPanel->m_hWnd, msg, wParam, lParam);
		}
		break;
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 300;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 555;
		break;
	case WM_SIZE:
		MoveWindow(pEdit1->m_hWnd, 10, 10, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit2->m_hWnd, 10, 50, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit3->m_hWnd, 10, 90, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit4->m_hWnd, 10, 130, LOWORD(lParam) - 158, HIWORD(lParam) - 324 + 90, TRUE);
		MoveWindow(pImageListPanel->m_hWnd, LOWORD(lParam) - 138, 130, 128, HIWORD(lParam) - 356 + 90, TRUE);
		MoveWindow(hCheckNsfw, LOWORD(lParam) - 10 - 128, HIWORD(lParam) - 226 + 90, 128, 32, TRUE);
		MoveWindow(hCombo, 10, HIWORD(lParam) - 184 + 90, LOWORD(lParam) - 20, 256, TRUE);
		MoveWindow(hButton, 10, HIWORD(lParam) - 142 + 90, LOWORD(lParam) - 20, 32, TRUE);
		break;
	case WM_COMMAND:
		if (HIWORD(wParam) == EN_CHANGE) {
			InvalidateRect((HWND)lParam, 0, 0);
			if ((pEdit1 && pEdit1->m_hWnd == (HWND)lParam) || (pEdit2 && pEdit2->m_hWnd == (HWND)lParam)) bModified = TRUE;
		}
		else if (LOWORD(wParam) == 1000) {
			std::vector<int> mediaIds;
			LPWSTR lpszServer = 0;
			LPWSTR lpszUserName = 0;
			LPWSTR lpszPassword = 0;
			LPWSTR lpszMessage = 0;
			if (bModified || !lstrlenW(pMastodon->m_szAccessToken)) {
				InternetSetOption(0, INTERNET_OPTION_END_BROWSER_SESSION, 0, 0);
				lpszServer = GetText(pEdit1->m_hWnd);
				URL_COMPONENTSW uc = { sizeof(uc) };
				uc.dwHostNameLength = (DWORD)GlobalSize(lpszServer);
				uc.lpszHostName = (LPWSTR)GlobalAlloc(0, uc.dwHostNameLength);
				if (InternetCrackUrlW(lpszServer, 0, 0, &uc)) {
					lstrcpyW(lpszServer, uc.lpszHostName);
				}
				GlobalFree(uc.lpszHostName);
				pMastodon->SetServer(lpszServer);
				if (!pMastodon->GetClientIDAndClientSecret()) goto END;
				lpszUserName = GetText(pEdit2->m_hWnd);
				lpszPassword = GetText(pEdit3->m_hWnd);
				if (!pMastodon->GetAccessToken(lpszUserName, lpszPassword)) goto END;
				bModified = FALSE;
			}
			int nImageCount = pImageListPanel->GetImageCount();
			for (int i = 0; i < nImageCount; ++i) {
				BitmapEx*pImage = pImageListPanel->GetImage(i);
				GUID guid1;
				LPWSTR lpszMediaType;
				if (pImage->GetRawFormat(&guid1) != Gdiplus::Ok) continue;
				if (guid1 == Gdiplus::ImageFormatGIF && pImage->m_lpByte) {
					lpszMediaType = L"image/gif";
				}
				else if (guid1 == Gdiplus::ImageFormatJPEG || guid1 == Gdiplus::ImageFormatEXIF) {
					lpszMediaType = L"image/jpeg";
				}
				else {
					lpszMediaType = L"image/png";
				}
				GUID guid2;
				GetEncoderClsid(lpszMediaType, &guid2);
				int nMediaID = 0;
				if (pImage->m_lpByte) {
					WCHAR szURL[256];
					pMastodon->MediaUpload(lpszMediaType, pImage->m_lpByte, (int)pImage->m_nSize, szURL, &nMediaID);
				}
				else {
					IStream *pStream = NULL;
					if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) == S_OK) {
						if (pImage->Save(pStream, &guid2) == S_OK) {
							ULARGE_INTEGER ulnSize;
							LARGE_INTEGER lnOffset;
							lnOffset.QuadPart = 0;
							if (pStream->Seek(lnOffset, STREAM_SEEK_END, &ulnSize) == S_OK) {
								if (pStream->Seek(lnOffset, STREAM_SEEK_SET, NULL) == S_OK) {
									LPBYTE baPicture = (LPBYTE)GlobalAlloc(0, (SIZE_T)ulnSize.QuadPart);
									ULONG ulBytesRead;
									pStream->Read(baPicture, (ULONG)ulnSize.QuadPart, &ulBytesRead);
									WCHAR szURL[256];
									pMastodon->MediaUpload(lpszMediaType, baPicture, (int)ulnSize.QuadPart, szURL, &nMediaID);
									GlobalFree(baPicture);
								}
							}
						}
						pStream->Release();
					}
				}
				if (nMediaID) {
					mediaIds.push_back(nMediaID);
				}
				else {
					WCHAR szText[512];
					wsprintf(szText, TEXT("トゥートの投稿に失敗しました。\r\n%d 番目の添付メディアのアップロードに失敗しました。"), i + 1);
					g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, 0, GetCurrentThreadId());
					MessageBoxW(hWnd, szText, L"確認", MB_ICONHAND);
					goto END;
				}
			}
			lpszMessage = GetText(pEdit4->m_hWnd);
			const int nVisibility = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
			LPCWSTR lpszVisibility = (LPCWSTR)SendMessageW(hCombo, CB_GETITEMDATA, nVisibility, 0);
			WCHAR szCreatedAt[32];
			if (!pMastodon->Toot(lpszMessage, lpszVisibility, szCreatedAt, mediaIds, (BOOL)SendMessageW(hCheckNsfw, BM_GETCHECK, 0, 0))) goto END;
			WCHAR szResult[1024];
			wsprintfW(szResult, L"投稿されました。\n投稿日時 = %s", szCreatedAt);
			g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, 0, GetCurrentThreadId());
			MessageBoxW(hWnd, szResult, L"確認", 0);
			pImageListPanel->ResetContent();
			SendMessageW(hCheckNsfw, BM_SETCHECK, 0, 0);
			SetWindowTextW(pEdit4->m_hWnd, 0);
			SetFocus(pEdit4->m_hWnd);
		END:
			GlobalFree(lpszServer);
			GlobalFree(lpszUserName);
			GlobalFree(lpszPassword);
			GlobalFree(lpszMessage);
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		delete pMastodon;
		delete pEdit1;
		delete pEdit2;
		delete pEdit3;
		delete pEdit4;
		delete pImageListPanel;
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	default:
		return DefDlgProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, 0);
	LPCWSTR lpszClassName = L"TootWindow";
	MSG msg = { 0 };
	const WNDCLASSW wndclass = { 0,WndProc,0,DLGWINDOWEXTRA,hInstance,LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)),LoadCursor(0,IDC_ARROW),0,0,lpszClassName };
	RegisterClassW(&wndclass);
	const HWND hWnd = CreateWindowW(lpszClassName, L"トゥートする", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, 0, 512, 800, 0, 0, hInstance, 0);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	ACCEL Accel[] = { { FVIRTKEY | FCONTROL,VK_RETURN,1000 } };
	HACCEL hAccel = CreateAcceleratorTable(Accel, _countof(Accel));
	while (GetMessage(&msg, 0, 0, 0)) {
		if (!TranslateAccelerator(hWnd, hAccel, &msg) && !IsDialogMessage(hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return (int)msg.wParam;
}