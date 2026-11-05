/****************************************************************************
**
** Copyright (C) 2014 UpdateNode UG (haftungsbeschränkt)
** Contact: code@updatenode.com
**
** This file is part of the UpdateNode Client.
**
** Commercial License Usage
** Licensees holding valid commercial UpdateNode license may use this file
** under the terms of the the Apache License, Version 2.0
** Full license description file: LICENSE.COM
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation. Please review the following information to ensure the
** GNU General Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
** Full license description file: LICENSE.GPL
**
****************************************************************************/

#include "WinCommander.hpp"
#include "WinArgumentQuote.hpp"

#include <QSysInfo>
#include <QDir>
#include <QUuid>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
#endif


/*!
Executes a command elevated specified by \apath , using paramters \aparameters.
\n
Parameter /aaWait decides if the function should return immediatelly after it's\n
execution or wait for the exit of the launched process
\n
Returns the return value of the executed command
*/
uint WinCommander::runProcessElevated(const QString &path,
                                      const QStringList &parameters,
                                      const QString &workingDir,
                                      int nShow, bool aWait) {
    uint result = 0;

#ifdef Q_OS_WIN
    QString params;
    HWND hwnd = NULL;
    LPCTSTR pszPath = (LPCTSTR)path.utf16();
    foreach(QString item, parameters)
        params += QString::fromStdWString(quoteWindowsArgument(item.toStdWString())) + " ";

    LPCTSTR pszParameters = (LPCTSTR)params.utf16();
    QString dir;
    if (workingDir.size() == 0)
        dir = QDir::toNativeSeparators(QDir::currentPath());
    else
        dir = QDir::toNativeSeparators(workingDir);
    LPCTSTR pszDirectory = (LPCTSTR)dir.utf16();

    SHELLEXECUTEINFO shex;
    DWORD dwCode  =   0;

    ZeroMemory(&shex, sizeof(shex));

    shex.cbSize       = sizeof(shex);
    shex.fMask        = SEE_MASK_NOCLOSEPROCESS;
    shex.hwnd         = hwnd;
    shex.lpVerb       = TEXT("runas");
    shex.lpFile       = pszPath;
    shex.lpParameters = pszParameters;
    shex.lpDirectory  = pszDirectory;
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
    shex.nShow        = nShow;

    ShellExecuteEx(&shex);
    if (shex.hProcess)
    {
        if(aWait)
        {
            WaitForSingleObject(shex.hProcess, INFINITE );
            GetExitCodeProcess(shex.hProcess, &dwCode);
        }
        CloseHandle (shex.hProcess) ;
    }
    else
        return -1;

    result = (uint)dwCode;
#else
    Q_UNUSED(path);
    Q_UNUSED(parameters);
    Q_UNUSED(workingDir);
    Q_UNUSED(aWait);
#endif
    return result;
}

// The pipe name is a rendezvous address, not an authorization secret. The
// server verifies the connecting process is the process returned by ShellExecute.
bool WinCommander::startElevatedWithPreflight(const QString &path, const QStringList &parameters) {
#ifdef Q_OS_WIN
    const QString pipeName = QStringLiteral("\\\\.\\pipe\\mintvpn-preflight-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    HANDLE pipe = CreateNamedPipeW(reinterpret_cast<LPCWSTR>(pipeName.utf16()),
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 1024, 1024, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return false;
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) { CloseHandle(pipe); return false; }
    OVERLAPPED op{}; op.hEvent = event;
    bool connected = ConnectNamedPipe(pipe, &op) != FALSE;
    DWORD connectError = connected ? ERROR_SUCCESS : GetLastError();
    if (!connected && connectError == ERROR_PIPE_CONNECTED) connected = true;
    if (!connected && connectError != ERROR_IO_PENDING) {
        CloseHandle(event); CloseHandle(pipe); return false;
    }
    QStringList childArgs = parameters;
    childArgs << "-mintvpn-preflight-pipe" << pipeName;
    QString args;
    for (const auto &item : childArgs)
        args += QString::fromStdWString(quoteWindowsArgument(item.toStdWString())) + " ";
    const QString workingDir = QDir::toNativeSeparators(QDir::currentPath());
    SHELLEXECUTEINFOW info{}; info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS; info.lpVerb = L"runas";
    info.lpFile = reinterpret_cast<LPCWSTR>(path.utf16());
    info.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
    info.lpDirectory = reinterpret_cast<LPCWSTR>(workingDir.utf16()); info.nShow = 1;
#ifdef MINTVPN_PREFLIGHT_TEST
    // Test seam: no UAC and no real profile; only built into the isolated harness.
    QString command = QString::fromStdWString(quoteWindowsArgument(path.toStdWString())) + " " + args;
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, reinterpret_cast<LPWSTR>(command.data()), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, reinterpret_cast<LPCWSTR>(workingDir.utf16()), &startup, &process)) {
        info.hProcess = process.hProcess; CloseHandle(process.hThread);
    }
    constexpr DWORD preflightTimeout = 1000;
#else
    ShellExecuteExW(&info);
    constexpr DWORD preflightTimeout = 30000;
#endif
    if (!info.hProcess) {
        CancelIoEx(pipe, &op); if (connectError == ERROR_IO_PENDING) GetOverlappedResult(pipe, &op, &connectError, TRUE);
        CloseHandle(event); CloseHandle(pipe); return false;
    }
    bool ready = false;
    if (!connected) {
        HANDLE waits[] = {info.hProcess, event};
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, preflightTimeout);
        DWORD transferred = 0;
        connected = wait == WAIT_OBJECT_0 + 1 && GetOverlappedResult(pipe, &op, &transferred, FALSE);
    }
    ULONG peerPid = 0;
    if (connected && GetNamedPipeClientProcessId(pipe, &peerPid) && peerPid == GetProcessId(info.hProcess)
        && WaitForSingleObject(info.hProcess, 0) == WAIT_TIMEOUT) {
        ResetEvent(event); OVERLAPPED reply{}; reply.hEvent = event;
        DWORD written = 0; const char ack = '1';
        BOOL sent = WriteFile(pipe, &ack, 1, &written, &reply);
        if (!sent && GetLastError() == ERROR_IO_PENDING) {
            HANDLE waits[] = {info.hProcess, event};
            sent = WaitForMultipleObjects(2, waits, FALSE, 3000) == WAIT_OBJECT_0 + 1
                && GetOverlappedResult(pipe, &reply, &written, FALSE);
            if (!sent) { CancelIoEx(pipe, &reply); GetOverlappedResult(pipe, &reply, &written, TRUE); }
        }
        ready = sent && written == 1 && WaitForSingleObject(info.hProcess, 0) == WAIT_TIMEOUT;
    }
    if (!connected && connectError == ERROR_IO_PENDING) {
        DWORD ignored = 0; CancelIoEx(pipe, &op); GetOverlappedResult(pipe, &op, &ignored, TRUE);
    }
    CloseHandle(info.hProcess); CloseHandle(event); CloseHandle(pipe);
    return ready;
#else
    Q_UNUSED(path); Q_UNUSED(parameters); return false;
#endif
}

bool WinCommander::completeElevatedPreflight(const QString &pipeName) {
#ifdef Q_OS_WIN
    if (!pipeName.startsWith(QStringLiteral("\\\\.\\pipe\\mintvpn-preflight-"))) return false;
    HANDLE pipe = CreateFileW(reinterpret_cast<LPCWSTR>(pipeName.utf16()), GENERIC_READ,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return false;
    char ack = 0; DWORD read = 0;
    const bool success = ReadFile(pipe, &ack, 1, &read, nullptr) && read == 1 && ack == '1';
    CloseHandle(pipe); return success;
#else
    Q_UNUSED(pipeName); return false;
#endif
}
