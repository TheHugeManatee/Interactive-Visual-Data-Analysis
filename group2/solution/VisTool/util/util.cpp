#include "util.h"

#include <Windows.h>

#include <DXUT.h>
#include <iostream>

std::wstring GetExePath()
{
	// get full path to .exe
	const size_t bufferSize = 1024;
	wchar_t buffer[bufferSize];
	if(0 == GetModuleFileNameW(nullptr, buffer, bufferSize))
	{
		return std::wstring(L"");
	}
	std::wstring path(buffer);
	// extract path (remove filename)
	size_t posSlash = path.find_last_of(L"/\\");
	if(posSlash != std::wstring::npos)
	{
		path = path.substr(0, posSlash + 1);
	}
	return path;
}

std::string GetPath(std::string path)
{
	// extract path (remove filename)
	size_t posSlash = path.find_last_of("/\\");
	if(posSlash != std::wstring::npos)
	{
		path = path.substr(0, posSlash + 1);
	}
	return path;
}

std::string GetFilename(std::string path, bool stripExt)
{
	// extract path (remove filename)
	size_t posSlash = path.find_last_of("/\\");
	if(posSlash != std::wstring::npos)
	{
		path = path.substr(posSlash+1);
	}

	if(stripExt) {
		size_t posDot = path.find_last_of(".");
		if(posDot != std::wstring::npos)
		{
			path = path.substr(0, posDot);
		}
	}

	return path;
}


void UpdateWindowTitle(const std::wstring& appName)
{
	// check if we should update the window title
	bool update = false;

	// update if window size changed
	static int s_windowWidth = 0;
	static int s_windowHeight = 0;
	if (s_windowWidth != DXUTGetWindowWidth() || s_windowHeight != DXUTGetWindowHeight()) {
		s_windowWidth = DXUTGetWindowWidth();
		s_windowHeight = DXUTGetWindowHeight();
		update = true;
	}

	// update if fps changed (updated once per second by DXUT)
	static float s_fps = 0.0f;
	static float s_mspf = 0.0f;
	if (s_fps != DXUTGetFPS()) {
		s_fps = DXUTGetFPS();
		s_mspf = 1000.0f / s_fps;
		update = true;
	}

	// update window title if something relevant changed
	if (update) {
		const size_t len = 512;
		wchar_t str[len];
		swprintf_s(str, len, L"%s %ux%u @ %.2f fps / %.2f ms", appName.c_str(), s_windowWidth, s_windowHeight, s_fps, s_mspf);
		SetWindowText(DXUTGetHWND(), str);
	}
}

static bool GetFilenameDialog(const std::wstring& lpstrTitle, const WCHAR* lpstrFilter, std::wstring &filename, const bool save, HWND owner, DWORD* nFilterIndex)
{
	BOOL result;
	OPENFILENAMEW ofn;
	ZeroMemory(&ofn,sizeof(OPENFILENAMEW));

	static WCHAR szFile[MAX_PATH];
	szFile[0] = 0;


	WCHAR szDir[MAX_PATH];
	if (filename.length()>0) {
		errno_t err=wcscpy_s(szDir,MAX_PATH,filename.c_str());
		filename.clear();
		if (err) return false;
	} else szDir[0]=0;
	ofn.lpstrInitialDir = szDir;

	//====== Dialog parameters
	ofn.lStructSize   = sizeof(OPENFILENAMEW);
	ofn.lpstrFilter   = lpstrFilter;
	ofn.nFilterIndex  = 1;
	ofn.lpstrFile     = szFile;
	ofn.nMaxFile      = sizeof(szFile);
	ofn.lpstrTitle    = lpstrTitle.c_str();
	ofn.nMaxFileTitle = sizeof (lpstrTitle.c_str());
	ofn.hwndOwner     = owner;

	if (save) {
		ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_OVERWRITEPROMPT;
		result = GetSaveFileNameW(&ofn);
	} else {
		ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_FILEMUSTEXIST;
		result = GetOpenFileNameW(&ofn);
	}
	if (result)	{
		filename = szFile;
		if (nFilterIndex != NULL) *nFilterIndex = ofn.nFilterIndex;
		return true;
	} else {
		filename = L"";
		return false;
	}
}

std::string ToAString(std::wstring s) 
{
	size_t l = s.length() + 1;
	char * b = new char[l+1];
	wcstombs(b, s.c_str(), l);
	std::string as(b);
	delete b;
	return as;
}

std::wstring ToWString(std::string s)
{
	size_t l = s.length() + 1;
	wchar_t * b = new wchar_t[l+1];
	mbstowcs(b, s.c_str(), l);
	std::wstring ws(b);
	delete b;
	return ws;
}

void PrintMatrix(XMMATRIX m)
{
	std::cout << m.r[0].m128_f32[0] << " " << m.r[0].m128_f32[1] << " " << m.r[0].m128_f32[2] << " " << m.r[0].m128_f32[3] << std::endl;
	std::cout << m.r[1].m128_f32[0] << " " << m.r[1].m128_f32[1] << " " << m.r[1].m128_f32[2] << " " << m.r[1].m128_f32[3] << std::endl;
	std::cout << m.r[2].m128_f32[0] << " " << m.r[2].m128_f32[1] << " " << m.r[2].m128_f32[2] << " " << m.r[2].m128_f32[3] << std::endl;
	std::cout << m.r[3].m128_f32[0] << " " << m.r[3].m128_f32[1] << " " << m.r[3].m128_f32[2] << " " << m.r[3].m128_f32[3] << std::endl;
}

void printVector(std::string label, XMVECTOR &v) {
	std::cout << label << ": " << v.m128_f32[0] << " " << v.m128_f32[1] << " " << v.m128_f32[2] << " " << v.m128_f32[3] << std::endl;
}