/**
 * @file Shortcut.cpp
 * @brief Writes the shortcut declared in Shortcut.h through the shell's link object.
 */
#include "Shortcut.h"

#include "LaunchArgs.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <filesystem>

namespace flow::ui {

// Write a .lnk at `link` that starts `exe` with `args`, in `folder`.
static HRESULT WriteShortcut(const std::wstring& link, const std::wstring& exe,
                             const std::wstring& args, const std::wstring& folder,
                             const std::wstring& description) {
    IShellLinkW* shell = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, (void**)&shell);
    if (FAILED(hr)) return hr;
    shell->SetPath(exe.c_str());
    shell->SetArguments(args.c_str());
    shell->SetWorkingDirectory(folder.c_str());
    shell->SetDescription(description.c_str());
    shell->SetIconLocation(exe.c_str(), 0);

    IPersistFile* file = nullptr;
    hr = shell->QueryInterface(IID_IPersistFile, (void**)&file);
    if (SUCCEEDED(hr)) {
        hr = file->Save(link.c_str(), TRUE);
        file->Release();
    }
    shell->Release();
    return hr;
}

void CreatePlayShortcut(HWND owner, const std::wstring& macro) {
    const std::filesystem::path macroPath(macro);

    // Offer "<macro name>.lnk" on the desktop.
    wchar_t file[MAX_PATH] = {0};
    const std::wstring name = macroPath.stem().wstring() + L".lnk";
    if (name.size() < MAX_PATH) wcscpy(file, name.c_str());
    wchar_t desktop[MAX_PATH] = {0};
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = desktop[0] ? desktop : NULL;
    ofn.lpstrFilter = L"Shortcuts (*.lnk)\0*.lnk\0";
    const std::wstring title = L"Shortcut to play " + macroPath.filename().wstring();
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = L"lnk";
    // Without NODEREFERENCELINKS the dialog would follow a picked shortcut to
    // its target, and the overwrite would land on that file instead.
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NODEREFERENCELINKS;
    if (!GetSaveFileNameW(&ofn)) return;

    wchar_t exe[MAX_PATH] = {0};
    const DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    HRESULT hr = (n == 0 || n >= MAX_PATH) ? E_FAIL : S_OK;
    if (SUCCEEDED(hr)) {
        // The file dialog may already have started COM on this thread; each
        // successful start, even S_FALSE, is balanced by its own uninit.
        const HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        hr = WriteShortcut(file, exe, PlayArguments(macro),
                           macroPath.parent_path().wstring(),
                           L"Play " + macroPath.filename().wstring() + L" with FLOW");
        if (SUCCEEDED(com)) CoUninitialize();
    }
    if (FAILED(hr)) {
        MessageBoxW(owner, L"Couldn't create the shortcut there.",
                    L"Create Shortcut", MB_OK | MB_ICONWARNING);
    }
}

}  // namespace flow::ui
