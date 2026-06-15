#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "Consts.h"

#define TOML_EXCEPTIONS 0
#define TOML_HEADER_ONLY 0
#define TOML_IMPLEMENTATION
#include <toml++/toml.hpp>

#include "ConfigParse.h"

namespace
{
constexpr std::array<const wchar_t*, 5> EXECUTABLES = {
    L"RainbowSixGame.exe",
    L"RainbowSix_DX11.exe",
    L"RainbowSix_DX12.exe",
    L"RainbowSix.exe",
    L"RainbowSix_Vulkan.exe",
};

constexpr DWORD POLL_INTERVAL_MS = 1000;
constexpr DWORD STARTUP_TIMEOUT_MS = 120000;
constexpr int REQUIRED_EMPTY_POLLS = 3;

std::wstring widen(std::string_view text)
{
    if (text.empty())
    {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

struct LaunchConfig
{
    std::wstring args;
    std::vector<std::wstring> tools;
};

LaunchConfig read_launch_config()
{
    LaunchConfig config;
    config.args = widen(CONSTS::DEFAULT_ARGS);

    const auto parsed = tbl::parse_config(CONSTS::DEFAULT_CONFIG_FILENAME);
    if (!parsed)
    {
        return config;
    }

    const auto& table = parsed.table();
    if (const auto value = table["Launch"]["args"].value<std::string>(); value && !value->empty())
    {
        config.args = widen(*value);
    }
    if (const auto* tools = table["Launch"]["tools"].as_array())
    {
        for (const auto& entry : *tools)
        {
            if (const auto path = entry.value<std::string>(); path && !path->empty())
            {
                config.tools.push_back(widen(*path));
            }
        }
    }
    return config;
}

void start_tool(const std::wstring& path)
{
    std::wstring command_line = L"\"" + path + L"\"";

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};

    if (CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup_info,
                       &process_info) != FALSE)
    {
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
        return;
    }

    if (GetLastError() != ERROR_ELEVATION_REQUIRED)
    {
        return;
    }

    wchar_t full_path[MAX_PATH];
    const wchar_t* file = path.c_str();
    if (GetFullPathNameW(path.c_str(), MAX_PATH, full_path, nullptr) != 0)
    {
        file = full_path;
    }

    SHELLEXECUTEINFOW shell_info{};
    shell_info.cbSize = sizeof(shell_info);
    shell_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    shell_info.lpVerb = L"runas";
    shell_info.lpFile = file;
    shell_info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&shell_info) != FALSE && shell_info.hProcess != nullptr)
    {
        CloseHandle(shell_info.hProcess);
    }
}

void set_working_directory_to_executable()
{
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return;
    }
    std::wstring directory(path, length);
    const auto separator = directory.find_last_of(L"\\/");
    if (separator != std::wstring::npos)
    {
        directory.resize(separator);
        SetCurrentDirectoryW(directory.c_str());
    }
}

bool is_game_process(const wchar_t* executable)
{
    for (const auto name : EXECUTABLES)
    {
        if (lstrcmpiW(executable, name) == 0)
        {
            return true;
        }
    }
    return false;
}

struct WindowQuery
{
    const std::vector<DWORD>* pids;
    bool present;
};

BOOL CALLBACK on_window(HWND window, LPARAM parameter)
{
    auto* query = reinterpret_cast<WindowQuery*>(parameter);
    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr)
    {
        return TRUE;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    for (const DWORD game_pid : *query->pids)
    {
        if (game_pid == pid)
        {
            query->present = true;
            return FALSE;
        }
    }
    return TRUE;
}

bool game_window_present()
{
    std::vector<DWORD> pids;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (is_game_process(entry.szExeFile))
            {
                pids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (pids.empty())
    {
        return false;
    }

    WindowQuery query{&pids, false};
    EnumWindows(on_window, reinterpret_cast<LPARAM>(&query));
    return query.present;
}

void terminate_game_processes()
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (is_game_process(entry.szExeFile))
            {
                const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (process != nullptr)
                {
                    TerminateProcess(process, 0);
                    CloseHandle(process);
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

const wchar_t* find_installed_executable()
{
    for (const auto executable : EXECUTABLES)
    {
        const DWORD attributes = GetFileAttributesW(executable);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return executable;
        }
    }
    return nullptr;
}

bool start_game(const wchar_t* executable, const std::wstring& args)
{
    std::wstring command_line = L"\"";
    command_line += executable;
    command_line += L"\" ";
    command_line += args;

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};

    if (CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup_info,
                       &process_info) == FALSE)
    {
        return false;
    }
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return true;
}

void wait_until_game_closed()
{
    const ULONGLONG started = GetTickCount64();
    while (!game_window_present())
    {
        if (GetTickCount64() - started > STARTUP_TIMEOUT_MS)
        {
            return;
        }
        Sleep(POLL_INTERVAL_MS);
    }

    int empty_polls = 0;
    while (empty_polls < REQUIRED_EMPTY_POLLS)
    {
        Sleep(POLL_INTERVAL_MS);
        empty_polls = game_window_present() ? 0 : empty_polls + 1;
    }
}

void show_error(const std::wstring& message)
{
    MessageBoxW(nullptr, message.c_str(), L"ThrowbackLoader", MB_OK | MB_ICONERROR);
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    set_working_directory_to_executable();
    terminate_game_processes();

    const LaunchConfig config = read_launch_config();

    const wchar_t* executable = find_installed_executable();
    if (executable == nullptr)
    {
        show_error(L"No Rainbow Six Siege executable was found in this folder.");
        return 1;
    }

    for (const auto& tool : config.tools)
    {
        start_tool(tool);
    }

    if (!start_game(executable, config.args))
    {
        show_error(std::wstring(L"Failed to launch ") + executable + L".");
        return 1;
    }

    wait_until_game_closed();
    terminate_game_processes();
    return 0;
}
