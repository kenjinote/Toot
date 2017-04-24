#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "wininet")
#pragma comment(lib, "gdiplus")

#include <windows.h>
#include <wininet.h>
#include <gdiplus.h>
#include <string>
#include <list>
#include <vector>
#include "resource.h"

std::wstring trim(const std::wstring& string, LPCWSTR trimCharacterList = L" \"\t\v\r\n")
{
	std::wstring result;
	std::wstring::size_type left = string.find_first_not_of(trimCharacterList);
	if (left != std::wstring::npos) {
		std::wstring::size_type right = string.find_last_not_of(trimCharacterList);
		result = string.substr(left, right - left + 1);
	}
	return result;
}

BOOL GetValueFromJSON(LPCWSTR lpszJson, LPCWSTR lpszKey, LPWSTR lpszValue)
{
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
	value = trim(value);
	lstrcpyW(lpszValue, value.c_str());
	return TRUE;
}

LPWSTR Post(LPCWSTR lpszServer, LPCWSTR lpszPath, LPCWSTR lpszHeader, LPBYTE lpbyData, int nSize)
{
	LPWSTR lpszReturn = 0;
	const HINTERNET hInternet = InternetOpen(TEXT("WinInet Toot Program"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL) goto END1;
	const HINTERNET hHttpSession = InternetConnectW(hInternet, lpszServer, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
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

BOOL GetClientIDAndClientSecret(HWND hEditOutput, LPCWSTR lpszServer, LPWSTR lpszID, LPWSTR lpszSecret)
{
	BOOL bRetutnValue = FALSE;
	CHAR szData[1024];
	lstrcpyA(szData, "client_name=TootApp&redirect_uris=urn:ietf:wg:oauth:2.0:oob&scopes=write");
	LPWSTR lpszReturn = Post(lpszServer, L"/api/v1/apps", L"Content-Type: application/x-www-form-urlencoded", (LPBYTE)szData, lstrlenA(szData));
	if (lpszReturn) {
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)lpszReturn);
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
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
	DWORD dwTextLen = WideCharToMultiByte(CP_UTF8, 0, szData, -1, 0, 0, 0, 0);
	LPSTR lpszDataA = (LPSTR)GlobalAlloc(GPTR, dwTextLen);
	WideCharToMultiByte(CP_UTF8, 0, szData, -1, lpszDataA, dwTextLen, 0, 0);
	LPWSTR lpszReturn = Post(lpszServer, L"/oauth/token", L"Content-Type: application/x-www-form-urlencoded", (LPBYTE)lpszDataA, dwTextLen - 1);
	GlobalFree(lpszDataA);
	if (lpszReturn) {
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)lpszReturn);
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
		bRetutnValue = GetValueFromJSON(lpszReturn, L"access_token", lpszAccessToken);
		GlobalFree(lpszReturn);
	}
	return bRetutnValue;
}

BOOL Toot(HWND hEditOutput, LPCWSTR lpszServer, LPCWSTR lpszAccessToken, LPCWSTR lpszMessage, LPCWSTR lpszVisibility, LPWSTR lpszCreatedAt, const std::vector<int> &mediaIds, BOOL bCheckNsfw)
{
	BOOL bRetutnValue = FALSE;
	WCHAR szData[1024];
	wsprintfW(szData, L"access_token=%s&status=%s&visibility=%s", lpszAccessToken, lpszMessage, lpszVisibility);
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
	LPWSTR lpszReturn = Post(lpszServer, L"/api/v1/statuses", L"Content-Type: application/x-www-form-urlencoded", (LPBYTE)lpszDataA, dwTextLen);
	GlobalFree(lpszDataA);
	if (lpszReturn) {
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)lpszReturn);
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
		bRetutnValue = GetValueFromJSON(lpszReturn, L"created_at", lpszCreatedAt);
		GlobalFree(lpszReturn);
	}
	return bRetutnValue;
}

VOID MakeBoundary(LPWSTR lpszBoundary)
{
	lstrcpy(lpszBoundary, L"----Boundary");
	lstrcat(lpszBoundary, L"kQcRxxn2b2BGpt9a");
}

BOOL MediaUpload(HWND hEditOutput, LPCWSTR lpszServer, LPCWSTR lpszAccessToken, LPCWSTR lpszMediaType ,LPBYTE lpbyImageData, int nDataLength, LPWSTR lpszTextURL, int *pMediaID)
{
	BOOL bRetutnValue = FALSE;
	WCHAR szBoundary[32];
	MakeBoundary(szBoundary);
	WCHAR szHeader[256];
	wsprintfW(szHeader, L"Authorization: Bearer %s\r\nContent-Type: multipart/form-data; boundary=%s", lpszAccessToken, szBoundary);
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
	LPWSTR lpszReturn = Post(lpszServer, L"/api/v1/media", szHeader, lpbyData, nTotalSize);
	GlobalFree(lpbyData);
	if (lpszReturn) {
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)lpszReturn);
		SendMessageW(hEditOutput, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
		bRetutnValue = GetValueFromJSON(lpszReturn, L"text_url", lpszTextURL);
		if (bRetutnValue) {
			WCHAR szMediaID[16];
			bRetutnValue = GetValueFromJSON(lpszReturn, L"id", szMediaID);
			if (bRetutnValue) {
				*pMediaID = _wtol(szMediaID);
			}
		}
		GlobalFree(lpszReturn);
	}
	return bRetutnValue;
}

class EditBox
{
	WNDPROC fnEditWndProc;
	LPWSTR m_lpszPlaceholder;
	int m_nLimit;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		EditBox* _this = (EditBox*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (_this) {
			if (msg == WM_PAINT) {
				const LRESULT lResult = CallWindowProc(_this->fnEditWndProc, hWnd, msg, wParam, lParam);
				const int nTextLength = GetWindowTextLength(hWnd);
				if ((_this->m_lpszPlaceholder && !nTextLength) || _this->m_nLimit) {
					const HDC hdc = GetDC(hWnd);
					const COLORREF OldColor = SetTextColor(hdc, RGB(180, 180, 180));
					const HFONT hOldFont = (HFONT)SelectObject(hdc, (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0));
					const int nLeft = LOWORD(SendMessage(hWnd, EM_GETMARGINS, 0, 0));
					if (_this->m_lpszPlaceholder && !nTextLength) {
						TextOutW(hdc, nLeft + 4, 2, _this->m_lpszPlaceholder, lstrlenW(_this->m_lpszPlaceholder));
					}
					if (_this->m_nLimit) {
						RECT rect;
						GetClientRect(hWnd, &rect);
						TCHAR szText[16];
						wsprintf(szText, TEXT("%5d"), _this->m_nLimit - nTextLength);
						DrawText(hdc, szText, -1, &rect, DT_RIGHT | DT_SINGLELINE | DT_BOTTOM);
					}
					SelectObject(hdc, hOldFont);
					SetTextColor(hdc, OldColor);
					ReleaseDC(hWnd, hdc);
				}
				return lResult;
			} else if (msg == WM_CHAR && wParam == 1) {
				SendMessage(hWnd, EM_SETSEL, 0, -1);
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
			m_lpszPlaceholder = (LPWSTR)GlobalAlloc(0, sizeof(TCHAR) * (lstrlen(lpszPlaceholder) + 1));
			lstrcpy(m_lpszPlaceholder, lpszPlaceholder);
		}
		m_hWnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", lpszDefaultText, dwStyle, x, y, width, height, hParent, hMenu, GetModuleHandle(0), 0);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
		fnEditWndProc = (WNDPROC)SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
	}
	~EditBox() { DestroyWindow(m_hWnd); GlobalFree(m_lpszPlaceholder); }
	void SetLimit(int nLimit) {
		SendMessage(m_hWnd, EM_LIMITTEXT, nLimit, 0);
		m_nLimit = nLimit;
	}
};

class ImageListPanel
{
	BOOL m_bDrag;
	int m_nDragIndex;
	int m_nSplitPrevIndex;
	int m_nSplitPrevPosY;
	int m_nMargin;
	int m_nImageMaxCount;
	HFONT m_hFont;
	std::list<Gdiplus::Bitmap*> m_listBitmap;
	WNDPROC fnWndProc;
	BOOL MoveImage(int nIndexFrom, int nIndexTo)
	{
		if (nIndexFrom < 0) nIndexFrom = 0;
		if (nIndexTo < 0) nIndexTo = 0;
		if (nIndexFrom == nIndexTo) return FALSE;
		std::list<Gdiplus::Bitmap*>::iterator itFrom = m_listBitmap.begin();
		std::list<Gdiplus::Bitmap*>::iterator itTo = m_listBitmap.begin();
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
				TCHAR szFileName[MAX_PATH];
				UINT iFile, nFiles;
				nFiles = DragQueryFile((HDROP)hDrop, 0xFFFFFFFF, NULL, 0);
				BOOL bUpdate = FALSE;
				for (iFile = 0; iFile<nFiles; ++iFile) {
					DragQueryFile(hDrop, iFile, szFileName, sizeof(szFileName));
					Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromFile(szFileName);
					if (pBitmap && pBitmap->GetLastStatus() == Gdiplus::Ok) {
						if ((int)_this->m_listBitmap.size() < _this->m_nImageMaxCount) {
							_this->m_listBitmap.push_back(pBitmap);
							bUpdate = TRUE;
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
						g.DrawString(L"画像をドロップ", -1, &font, rectf, &f, &Gdiplus::SolidBrush(Gdiplus::Color::MakeARGB(128, 0, 0, 0)));
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
				}
				EndPaint(hWnd, &ps);
			}
			return 0;
			case WM_LBUTTONDOWN:
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				POINT point = { LOWORD(lParam), HIWORD(lParam) };
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
			}
			return 0;
			case WM_MOUSEMOVE:
				if (_this->m_bDrag)
				{
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
				if (_this->m_bDrag)
				{
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
	void RemoveAllImage()
	{
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
	{
		WNDCLASSW wndclass = { CS_HREDRAW | CS_VREDRAW,WndProc,0,0,GetModuleHandle(0),0,LoadCursor(0,IDC_ARROW),(HBRUSH)(COLOR_WINDOW + 1),0,__FUNCTIONW__ };
		RegisterClassW(&wndclass);
		m_hWnd = CreateWindowW(__FUNCTIONW__, 0, dwStyle, x, y, width, height, hParent, 0, GetModuleHandle(0), this);
	}
	~ImageListPanel()
	{
		RemoveAllImage();
	}
	int GetImageCount() { return (int)m_listBitmap.size(); }
	Gdiplus::Bitmap * GetImage(int nIndex) { 
		std::list<Gdiplus::Bitmap*>::iterator it = m_listBitmap.begin();
		std::advance(it, nIndex);
		return *it;
	}
	void ResetContent()
	{
		RemoveAllImage();
		InvalidateRect(m_hWnd, 0, 1);
	}
};

HHOOK g_hHook;
LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HCBT_ACTIVATE) {
		UnhookWindowsHookEx(g_hHook);
		RECT rectMessageBox, rectParentWnd;
		GetWindowRect((HWND)wParam, &rectMessageBox);
		GetWindowRect(GetParent((HWND)wParam), &rectParentWnd);
		SetWindowPos((HWND)wParam, 0,
			(rectParentWnd.right + rectParentWnd.left - rectMessageBox.right + rectMessageBox.left) >> 1,
			(rectParentWnd.bottom + rectParentWnd.top - rectMessageBox.bottom + rectMessageBox.top) >> 1,
			0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	return 0;
}

BOOL GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;
	UINT  size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return FALSE;
	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(GlobalAlloc(0,size));
	if (pImageCodecInfo == NULL) return FALSE;
	GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT i = 0; i < num; ++i)
	{
		if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[i].Clsid;
			GlobalFree(pImageCodecInfo);
			return TRUE;
		}
	}
	GlobalFree(pImageCodecInfo);
	return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static EditBox *pEdit1, *pEdit2, *pEdit3, *pEdit4;
	static ImageListPanel *pImageListPanel;
	static HWND hEdit5, hCombo, hButton, hCheckNsfw;
	static HFONT hFont;
	static WCHAR szAccessToken[65];
	static BOOL bModified;
	switch (msg) {
	case WM_CREATE:
		hFont = CreateFont(24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Yu Gothic UI"));
		pEdit1 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, TEXT("サーバー名"));
		SendMessage(pEdit1->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit2 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, TEXT("メールアドレス"));
		SendMessage(pEdit2->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit3 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 0, 0, 0, 0, hWnd, 0, TEXT("パスワード"));
		SendMessage(pEdit3->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit4 = new EditBox(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN, 0, 0, 0, 0, hWnd, 0, TEXT("今何してる？"));
		SendMessage(pEdit4->m_hWnd, WM_SETFONT, (WPARAM)hFont, 0);
		pEdit4->SetLimit(500);
		pImageListPanel = new ImageListPanel(4, WS_VISIBLE | WS_CHILD | WS_BORDER, 0, 0, 0, 0, hWnd, hFont);
		hCheckNsfw = CreateWindow(TEXT("BUTTON"), TEXT("NSFWチェック"), WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hCheckNsfw, WM_SETFONT, (WPARAM)hFont, 0);
		hCombo = CreateWindow(TEXT("COMBOBOX"), 0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hCombo, CB_SETITEMDATA, SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("全体に公開")), (LPARAM)TEXT("public"));
		SendMessage(hCombo, CB_SETITEMDATA, SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("自分のタイムラインでのみ公開（公開タイムラインには表示しない）")), (LPARAM)TEXT("unlisted"));
		SendMessage(hCombo, CB_SETITEMDATA, SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("自分のフォロワーにのみ公開")), (LPARAM)TEXT("private"));
		SendMessage(hCombo, CB_SETITEMDATA, SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("非公開・ダイレクトメッセージ（自分と@ユーザーのみ閲覧可）")), (LPARAM)TEXT("direct"));
		SendMessage(hCombo, CB_SETCURSEL, 0, 0);
		hButton = CreateWindow(TEXT("BUTTON"), TEXT("トゥート!"), WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)1000, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hButton, WM_SETFONT, (WPARAM)hFont, 0);
		hEdit5 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), 0, WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hEdit5, WM_SETFONT, (WPARAM)hFont, 0);
		DragAcceptFiles(hWnd, TRUE);
		break;
	case WM_DROPFILES:
		if (pImageListPanel) {
			SendMessage(pImageListPanel->m_hWnd, msg, wParam, lParam);
		}
		break;
	case WM_SIZE:
		MoveWindow(pEdit1->m_hWnd, 10, 10, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit2->m_hWnd, 10, 50, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit3->m_hWnd, 10, 90, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(pEdit4->m_hWnd, 10, 130, LOWORD(lParam) - 158, HIWORD(lParam) - 324, TRUE);
		MoveWindow(pImageListPanel->m_hWnd, LOWORD(lParam) - 138, 130, 128, HIWORD(lParam) - 356, TRUE);
		MoveWindow(hCheckNsfw, LOWORD(lParam) - 10 - 128, HIWORD(lParam) - 226, 128, 32, TRUE);
		MoveWindow(hCombo, 10, HIWORD(lParam) - 184, LOWORD(lParam) - 20, 256, TRUE);
		MoveWindow(hButton, 10, HIWORD(lParam) - 142, LOWORD(lParam) - 20, 32, TRUE);
		MoveWindow(hEdit5, 10, HIWORD(lParam) - 100, LOWORD(lParam) - 20, 90, TRUE);
		break;
	case WM_COMMAND:
		if (HIWORD(wParam) == EN_CHANGE) {
			InvalidateRect((HWND)lParam, 0, 0);
			if ((pEdit1 && pEdit1->m_hWnd == (HWND)lParam) || (pEdit2 && pEdit2->m_hWnd == (HWND)lParam)) bModified = TRUE;
		}
		else if (LOWORD(wParam) == 1000) {
			SetWindowText(hEdit5, 0);
			WCHAR szServer[256] = { 0 };
			GetWindowTextW(pEdit1->m_hWnd, szServer, _countof(szServer));
			URL_COMPONENTSW uc = { sizeof(uc) };
			WCHAR szHostName[256] = { 0 };
			uc.lpszHostName = szHostName;
			uc.dwHostNameLength = _countof(szHostName);
			if (InternetCrackUrlW(szServer, 0, 0, &uc)) {
				lstrcpyW(szServer, szHostName);
			}
			if (bModified || !lstrlen(szAccessToken)) {
				InternetSetOption(0, INTERNET_OPTION_END_BROWSER_SESSION, 0, 0);
				WCHAR szUserName[256] = { 0 };
				GetWindowTextW(pEdit2->m_hWnd, szUserName, _countof(szUserName));
				WCHAR szPassword[256] = { 0 };
				GetWindowTextW(pEdit3->m_hWnd, szPassword, _countof(szPassword));
				WCHAR szClientID[65] = { 0 };
				WCHAR szSecret[65] = { 0 };
				if (!GetClientIDAndClientSecret(hEdit5, szServer, szClientID, szSecret)) return 0;
				if (!GetAccessToken(hEdit5, szServer, szClientID, szSecret, szUserName, szPassword, szAccessToken)) return 0;
				bModified = FALSE;
			}
			std::vector<int> mediaIds;
			{
				int nImageCount = pImageListPanel->GetImageCount();
				for (int i = 0; i < nImageCount; ++i)
				{
					Gdiplus::Bitmap*pImage = pImageListPanel->GetImage(i);
					GUID guid1, guid2;
					LPWSTR lpszMediaType = L"image/jpeg";
					if (pImage->GetRawFormat(&guid1) != Gdiplus::Ok) continue;
					if (guid1 == Gdiplus::ImageFormatPNG || guid1 == Gdiplus::ImageFormatMemoryBMP || guid1 == Gdiplus::ImageFormatBMP || guid1 == Gdiplus::ImageFormatTIFF || guid1 == Gdiplus::ImageFormatUndefined) {
						lpszMediaType = L"image/png";
					}
					else if (guid1 == Gdiplus::ImageFormatGIF)
					{
						lpszMediaType = L"image/gif";
					}
					GetEncoderClsid(lpszMediaType, &guid2);
					IStream *pStream = NULL;
					if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) == S_OK)
					{
						if (pImage->Save(pStream, &guid2) == S_OK)
						{
							ULARGE_INTEGER ulnSize;
							LARGE_INTEGER lnOffset;
							lnOffset.QuadPart = 0;
							if (pStream->Seek(lnOffset, STREAM_SEEK_END, &ulnSize) == S_OK)
							{
								if (pStream->Seek(lnOffset, STREAM_SEEK_SET, NULL) == S_OK)
								{
									LPBYTE baPicture = (LPBYTE)GlobalAlloc(0, (SIZE_T)ulnSize.QuadPart);
									ULONG ulBytesRead;
									pStream->Read(baPicture, (ULONG)ulnSize.QuadPart, &ulBytesRead);
									WCHAR szURL[256];
									int nMediaID = 0;
									if (MediaUpload(hEdit5, szServer, szAccessToken, lpszMediaType, (LPBYTE)baPicture, (int)ulnSize.QuadPart, szURL, &nMediaID))
									{
										mediaIds.push_back(nMediaID);
									}
									GlobalFree(baPicture);
								}
							}
						}
					}
					pStream->Release();
				}
			}
			WCHAR szMessage[501] = { 0 };
			GetWindowTextW(pEdit4->m_hWnd, szMessage, _countof(szMessage));
			const int nVisibility = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
			LPCWSTR lpszVisibility = (LPCWSTR)SendMessage(hCombo, CB_GETITEMDATA, nVisibility, 0);
			WCHAR szCreatedAt[32] = { 0 };
			if (!Toot(hEdit5, szServer, szAccessToken, szMessage, lpszVisibility, szCreatedAt, mediaIds, (BOOL)SendMessage(hCheckNsfw, BM_GETCHECK, 0, 0))) return 0;
			WCHAR szResult[1024];
			wsprintfW(szResult, L"投稿されました。\n投稿日時 = %s", szCreatedAt);
			g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, 0, GetCurrentThreadId());
			MessageBoxW(hWnd, szResult, L"確認", 0);
			pImageListPanel->ResetContent();
			SendMessage(hCheckNsfw, BM_SETCHECK, 0, 0);
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
		delete pImageListPanel;
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
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, 0);
	LPCWSTR lpszClassName = L"TootWindow";
	MSG msg = { 0 };
	const WNDCLASSW wndclass = { 0,WndProc,0,DLGWINDOWEXTRA,hInstance,LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)),LoadCursor(0,IDC_ARROW),0,0,lpszClassName };
	RegisterClassW(&wndclass);
	const HWND hWnd = CreateWindowW(lpszClassName, L"トゥートする", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 512, 800, 0, 0, hInstance, 0);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0)) {
		if (!IsDialogMessage(hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return (int)msg.wParam;
}