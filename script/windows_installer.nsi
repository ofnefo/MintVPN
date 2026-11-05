!ifndef APP_VERSION
  !define APP_VERSION "1.0.0"
!endif
!ifndef APP_VERSION_MAJOR
  !define APP_VERSION_MAJOR 1
!endif
!ifndef APP_VERSION_MINOR
  !define APP_VERSION_MINOR 0
!endif
!ifndef APP_VERSION_PATCH
  !define APP_VERSION_PATCH 0
!endif
!ifndef APP_VERSION_BUILD
  !define APP_VERSION_BUILD 0
!endif

; Verify every Cronet DLL embedded in this installer at compile time.
; Cronet x64 verification performed by package_worker.py before compilation

; The script is UTF-8 and contains Russian UI text. Without a Unicode NSIS
; target those strings are compiled through the active ANSI code page and turn
; into mojibake on another Windows locale.
Unicode true

Name "MintVPN ${APP_VERSION}"
OutFile "MintVPNSetup.exe"

; 1. NEVER ask for UAC on launch
RequestExecutionLevel user 

SetCompressor /SOLID /FINAL lzma
SetCompressorDictSize 64

!include MUI2.nsh
!include nsDialogs.nsh
!include LogicLib.nsh
!include FileFunc.nsh
!include WinVer.nsh
!include x64.nsh

!define APP_DIR_NAME "MintVPN"

!define MUI_ICON "res\Quattro.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Добро пожаловать в MintVPN"
!define MUI_WELCOMEPAGE_TEXT "Мастер установит MintVPN на этот компьютер."
!addplugindir .\script\

; This is the Windows constant used to draw the UAC Shield on a button
!ifndef BCM_SETSHIELD
  !define BCM_SETSHIELD 0x160C
!endif

Var IsAllUsers
Var ProfilePath
Var ProfileInput

; =====================================
; PAGE SEQUENCE
; =====================================
!insertmacro MUI_PAGE_WELCOME

; Custom pages do not use MUI_PAGE_CUSTOMFUNCTION_PRE.
; The skip logic is handled directly inside InstallModePageCreate.
Page custom ProfilePageCreate ProfilePageLeave

!define MUI_PAGE_CUSTOMFUNCTION_SHOW DirectoryShow
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!define MUI_DIRECTORYPAGE_TEXT_TOP "MintVPN будет установлен в указанную папку."
!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}.${APP_VERSION_BUILD}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "MintVPN"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "MintVPN Setup"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Quattro / Quattro contributors"

UninstallText "Удалить MintVPN с этого компьютера?"
UninstallIcon "res\Quattro.ico"

; =====================================
; INIT & SEAMLESS RESTART LOGIC
; =====================================
Function .onInit
  ; Personal side-by-side install only; preserve any existing Quattro install.
  StrCpy $IsAllUsers "0"
  StrCpy $INSTDIR "$LOCALAPPDATA\MintVPN\app"
  ReadRegStr $ProfilePath HKCU "Software\MintVPNStable" "ProfilePath"
  ; Explicit profile also works for /S, which skips the profile page.
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/PROFILE=" $1
  ${IfNot} ${Errors}
    StrCpy $ProfilePath $1
  ${EndIf}
  SetShellVarContext current
FunctionEnd


; =====================================
; CUSTOM JUST ME / ALL USERS PAGE
; =====================================


 ; Choose an existing profile or leave empty for the default new profile.
Function ProfilePageCreate
  !insertmacro MUI_HEADER_TEXT "Profile" "Choose the existing profile to keep, or use a new one."
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 22u "Existing profile directory (leave empty to use the default MintVPN profile):"
  Pop $0
  ${NSD_CreateText} 0 25u 76% 12u "$ProfilePath"
  Pop $ProfileInput
  ${NSD_CreateBrowseButton} 79% 25u 21% 12u "Browse..."
  Pop $0
  ${NSD_OnClick} $0 ProfileBrowse
  nsDialogs::Show
FunctionEnd

Function ProfileBrowse
  ${NSD_GetText} $ProfileInput $ProfilePath
  nsDialogs::SelectFolderDialog "Select the existing profile directory" "$ProfilePath"
  Pop $0
  ${If} $0 != error
    ${NSD_SetText} $ProfileInput $0
  ${EndIf}
FunctionEnd

Function ProfilePageLeave
  ${NSD_GetText} $ProfileInput $ProfilePath
  ${If} $ProfilePath == ""
    StrCpy $ProfilePath "$LOCALAPPDATA\MintVPN\profile"
  ${Else}
    ; Resolve before the working directory changes and validate the chosen profile.
    System::Call 'kernel32::GetFullPathNameW(w "$ProfilePath", i ${NSIS_MAX_STRLEN}, w .r2, p 0) i.r3'
    StrCpy $ProfilePath $2
    ; The default profile may not have been created until the first app launch.
    ; Any other chosen profile must already contain the expected database.
    ${If} $ProfilePath != "$LOCALAPPDATA\MintVPN\profile"
      IfFileExists "$ProfilePath\config\quattro.db" +3 0
      MessageBox MB_OK|MB_ICONSTOP "The selected directory does not contain config\quattro.db."
      Abort
    ${EndIf}
  ${EndIf}
  ; Avoid Windows argument parsing ambiguity from a final backslash.
  StrCpy $0 $ProfilePath 1 -1
  ${If} $0 == '\'
    StrCpy $ProfilePath $ProfilePath -1
    StrCpy $ProfilePath "$ProfilePath/"
  ${EndIf}
FunctionEnd

; =====================================
; DIRECTORY PAGE & UAC SHIELD
; =====================================
Function DirectoryShow
  ; Target the "Next" (Install) button
  GetDlgItem $0 $HWNDPARENT 1

  ${If} $IsAllUsers == "1"
    SendMessage $0 ${BCM_SETSHIELD} 0 1 ; Draw Shield
  ${Else}
    SendMessage $0 ${BCM_SETSHIELD} 0 0 ; Remove Shield
  ${EndIf}
FunctionEnd

Function EnsureAppSubfolder
  ; A hand-typed "D:\Apps\" would otherwise append into "D:\Apps\\Quattro".
  StrCpy $0 $INSTDIR "" -1
  ${If} $0 == '\'
    StrCpy $INSTDIR $INSTDIR -1
  ${EndIf}

  ${GetFileName} "$INSTDIR" $0
  ${If} $0 != "${APP_DIR_NAME}"
    StrCpy $INSTDIR "$INSTDIR\${APP_DIR_NAME}"
  ${EndIf}
FunctionEnd

Function DirectoryLeave
  System::Call 'kernel32::GetFullPathNameW(w "$INSTDIR", i ${NSIS_MAX_STRLEN}, w .r2, p 0) i.r3'
  StrCpy $INSTDIR $2
  ; A fresh directory is required: never replace a running or existing client.
  IfFileExists "$INSTDIR" 0 +3
  MessageBox MB_OK|MB_ICONSTOP "Choose a new installation directory that does not exist yet."
  Abort
FunctionEnd

!macro RequireInstalledClientsStopped
  ; Refuse removal if a matching process cannot be inspected or belongs here.
  System::Call 'kernel32::SetEnvironmentVariableW(w "MINTVPN_INSTALL_CHECK", w "$INSTDIR") i.r0'
  InitPluginsDir
  File /oname=$PLUGINSDIR\installer-process-check.ps1 "script\installer-process-check.ps1"
  nsExec::ExecToStack '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\installer-process-check.ps1"' 
  Pop $R0
  Pop $R1
  System::Call 'kernel32::SetEnvironmentVariableW(w "MINTVPN_INSTALL_CHECK", p 0) i.r0'
  ${If} $R0 != 0
    IfSilent +2
    MessageBox MB_OK|MB_ICONSTOP "Close the client installed in this folder before uninstalling."
    Abort
  ${EndIf}
!macroend

!macro DeleteOwnedShortcut link
  System::Call 'kernel32::SetEnvironmentVariableW(w "MINTVPN_SHORTCUT", w "${link}") i.r0'
  System::Call 'kernel32::SetEnvironmentVariableW(w "MINTVPN_TARGET", w "$INSTDIR\MintVPN.exe") i.r0'
  nsExec::ExecToStack '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -Command "if (Test-Path -LiteralPath $$env:MINTVPN_SHORTCUT) { $$s=(New-Object -ComObject WScript.Shell).CreateShortcut($$env:MINTVPN_SHORTCUT); if ($$s.TargetPath -eq $$env:MINTVPN_TARGET) { Remove-Item -LiteralPath $$env:MINTVPN_SHORTCUT } }"'
  Pop $R0
  Pop $R1
  System::Call 'kernel32::SetEnvironmentVariableW(w "MINTVPN_SHORTCUT", p 0) i.r0'
  System::Call 'kernel32::SetEnvironmentVariableW(w "MINTVPN_TARGET", p 0) i.r0'
!macroend

; =====================================
; INSTALLATION
; =====================================
Section "Install"
  ; Silent/elevated flows must not silently fall back to an unrelated profile.
  ${If} $ProfilePath == ""
    Abort "A profile must be selected before installation."
  ${EndIf}
  ; Never overwrite another install or another application's shortcuts.
  ; Preserve an existing uninstall registration, even if its values are empty.
  System::Call 'advapi32::RegOpenKeyExW(p 0x80000001, w "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable", i 0, i 0x20019, *p .r1) i.r0'
  ${If} $0 == 0
    System::Call 'advapi32::RegCloseKey(p r1)'
    Abort "MintVPN Stable uninstall registration already exists."
  ${ElseIf} $0 != 2
    Abort "Cannot verify MintVPN Stable uninstall registration."
  ${EndIf}
  ReadRegStr $0 HKCU "Software\MintVPNStable" "InstallPath"
  ${If} $0 != ""
    Abort "MintVPN Stable is already registered. Remove that installation first."
  ${EndIf}
  IfFileExists "$DESKTOP\MintVPN Stable.lnk" install_conflict 0
  IfFileExists "$SMPROGRAMS\MintVPN Stable.lnk" install_conflict 0
  Goto install_no_conflict
install_conflict:
  Abort "A MintVPN Stable shortcut already exists."
install_no_conflict:
  System::Call 'kernel32::GetFullPathNameW(w "$INSTDIR", i ${NSIS_MAX_STRLEN}, w .r2, p 0) i.r3'
  ${If} $3 == 0
    Abort "Invalid installation directory."
  ${EndIf}
  StrCpy $INSTDIR $2
  System::Call 'kernel32::GetFullPathNameW(w "$ProfilePath", i ${NSIS_MAX_STRLEN}, w .r2, p 0) i.r3'
  ${If} $3 == 0
    Abort "Invalid profile directory."
  ${EndIf}
  StrCpy $ProfilePath $2
  ; Avoid a final backslash escaping the shortcut argument quote.
  StrCpy $0 $ProfilePath 1 -1
  ${If} $0 == "\"
    StrCpy $ProfilePath $ProfilePath -1
    StrCpy $ProfilePath "$ProfilePath/"
  ${EndIf}
  ; Validate even in silent mode, where custom page validation is skipped.
  ${If} $ProfilePath != "$LOCALAPPDATA\MintVPN\profile"
    IfFileExists "$ProfilePath\config\quattro.db" +2 0
    Abort "The selected directory does not contain config\quattro.db."
  ${EndIf}
  ; Reject overlapping profile/install trees, including a not-yet-created default profile.
  StrLen $0 "$INSTDIR\"
  StrCpy $1 "$ProfilePath\" $0
  StrCmp $1 "$INSTDIR\" profile_overlap
  StrLen $0 "$ProfilePath\"
  StrCpy $1 "$INSTDIR\" $0
  StrCmp $1 "$ProfilePath\" profile_overlap
  ${GetParent} "$INSTDIR" $0
  ClearErrors
  CreateDirectory "$0"
  IfErrors install_failed
  ; Atomic creation rejects existing paths, even in silent mode or after the page check.
  System::Call 'kernel32::CreateDirectoryW(w "$INSTDIR", p 0) i.r0'
  StrCmp $0 0 install_failed
  Goto install_ready
profile_overlap:
  Abort "The installation directory must not overlap the profile."
install_failed:
  Abort "Could not create a new installation directory."
install_ready:
  SetOutPath "$INSTDIR"
  SetOverwrite off

!ifdef QUATTRO_LOCAL_X64
  ; Reproducible local package for the current Windows x64 target. Release CI
  ; omits this define and keeps producing the multi-architecture installer.
  ${IfNot} ${IsNativeAMD64}
    Abort "This Quattro build requires 64-bit Windows on an x64 processor."
  ${EndIf}
  File /oname=libcronet.dll "deployment\windows-amd64\libcronet.dll"
  File /oname=MintVPNCore.exe "deployment\windows-amd64\MintVPNCore.exe"
  File /oname=MintVPN.exe "deployment\windows-amd64\MintVPN.exe"
  File /oname=QuattroUpdater.exe "deployment\windows-amd64\QuattroUpdater.exe"
!else
  ${If} ${IsNativeAMD64}
    ${If} ${AtLeastWaaS} 1809
      File /oname=libcronet.dll "deployment\windows-amd64\libcronet.dll"
      File /oname=MintVPNCore.exe "deployment\windows-amd64\MintVPNCore.exe"
      File /oname=MintVPN.exe "deployment\windows-amd64\MintVPN.exe"
      File /oname=QuattroUpdater.exe "deployment\windows-amd64\QuattroUpdater.exe"
    ${Else}
      File /oname=MintVPNCore.exe "deployment\windowslegacy-amd64\MintVPNCore.exe"
      File /oname=MintVPN.exe "deployment\windowslegacy-amd64\MintVPN.exe"
      File /oname=QuattroUpdater.exe "deployment\windowslegacy-amd64\QuattroUpdater.exe"
    ${EndIf}
  ${ElseIf} ${IsNativeARM64}
    File /oname=libcronet.dll "deployment\windows-arm64\libcronet.dll"
    File /oname=MintVPNCore.exe "deployment\windows-arm64\MintVPNCore.exe"
    File /oname=MintVPN.exe "deployment\windows-arm64\MintVPN.exe"
    File /oname=QuattroUpdater.exe "deployment\windows-arm64\QuattroUpdater.exe"
  ${ElseIf} ${IsNativeIA32}
    File /oname=MintVPNCore.exe "deployment\windowslegacy-386\MintVPNCore.exe"
    File /oname=MintVPN.exe "deployment\windowslegacy-386\MintVPN.exe"
    File /oname=QuattroUpdater.exe "deployment\windowslegacy-386\QuattroUpdater.exe"
  ${Else}
    Abort "Unsupported CPU architecture!"
  ${EndIf}
!endif

  ; Include available upstream notices with each architecture's binary distribution.
  SetOutPath "$INSTDIR\licenses"
  File /oname=MintVPN-GPL-3.0.txt "LICENSE"
  File /oname=Upstream-Notice.md "QUATTRO_NOTICE.md"
  File /oname=QHotkey-BSD-3-Clause.txt "3rdparty\QHotkey\LICENSE"
  File /oname=SQLiteCpp-LICENSE.txt "3rdparty\SQLiteCpp\LICENSE.txt"
  File /oname=simple-protobuf-LICENSE.txt "3rdparty\simple-protobuf\LICENSE"
  SetOutPath "$INSTDIR\licenses\qt-6.11.1-sbom"
  File /r "3rdparty\qt-6.11.1-sbom\*"
  SetOutPath "$INSTDIR\licenses\go-core"
  File /r "3rdparty\go-core-notices\*"
  SetOutPath "$INSTDIR\licenses\additional"
  File /r "3rdparty\mintvpn-notices\*"
  SetOutPath "$INSTDIR"

  ; Profile is selected locally; never embed a user's path in the installer source.
  WriteRegStr HKCU "Software\MintVPNStable" "ProfilePath" "$ProfilePath"
  SetShellVarContext current
  CreateShortcut "$DESKTOP\MintVPN Stable.lnk" "$INSTDIR\MintVPN.exe" '-appdata "$ProfilePath"' "$INSTDIR\MintVPN.exe" 0
  CreateShortcut "$SMPROGRAMS\MintVPN Stable.lnk" "$INSTDIR\MintVPN.exe" '-appdata "$ProfilePath"' "$INSTDIR\MintVPN.exe" 0
  ${If} $IsAllUsers == "1"
    SetShellVarContext all
  ${EndIf}

  WriteRegStr SHCTX "Software\MintVPNStable" "InstallPath" "$INSTDIR"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "DisplayName" "MintVPN"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "Publisher" "MintVPN"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "DisplayIcon" "$INSTDIR\MintVPN.exe"
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "VersionMajor" ${APP_VERSION_MAJOR}
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "VersionMinor" ${APP_VERSION_MINOR}
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "InstallLocation" "$INSTDIR"
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "NoModify" 1
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

; =====================================
; UNINSTALLER
; =====================================
Function un.onInit
  ; This package only creates current-user installations.
  StrCpy $IsAllUsers "0"
  SetShellVarContext current
FunctionEnd

Section "Uninstall"
  !insertmacro RequireInstalledClientsStopped

  ; Shortcuts belong to the current profile owner, even for an all-users binary.
  SetShellVarContext current
  ReadRegStr $0 HKCU "Software\MintVPNStable" "InstallPath"
  ${If} $0 == $INSTDIR
    !insertmacro DeleteOwnedShortcut "$SMPROGRAMS\MintVPN Stable.lnk"
    !insertmacro DeleteOwnedShortcut "$DESKTOP\MintVPN Stable.lnk"
  ${EndIf}
  ${If} $IsAllUsers == "1"
    SetShellVarContext all
  ${EndIf}

  Delete "$INSTDIR\libcronet.dll"
  Delete "$INSTDIR\MintVPNCore.exe"
  Delete "$INSTDIR\MintVPN.exe"
  Delete "$INSTDIR\QuattroUpdater.exe"
  Delete "$INSTDIR\QuattroUpdater.old.exe"
  Delete "$INSTDIR\uninstall.exe"

  !include "script\installer-license-removal.nsh"
  RMDir "$INSTDIR"

  ; Never remove a newer or different installation's registration.
  ReadRegStr $0 HKCU "Software\MintVPNStable" "InstallPath"
  ${If} $0 == $INSTDIR
    DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\MintVPNStable"
    DeleteRegValue SHCTX "Software\MintVPNStable" "InstallPath"
  ${EndIf}

  ; Existing profile data is never removed by this uninstaller.
SectionEnd
