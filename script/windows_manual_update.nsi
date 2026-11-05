!ifndef APP_VERSION
  !define APP_VERSION "0.0.0"
!endif
!ifndef APP_VERSION_MAJOR
  !define APP_VERSION_MAJOR 0
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
!ifndef UPDATE_ARCHIVE
  !define UPDATE_ARCHIVE "deployment\manual-update\Quattro.zip"
!endif

Unicode true

Name "Quattro Update ${APP_VERSION}"
OutFile "QuattroUpdate-${APP_VERSION}.exe"
RequestExecutionLevel user

; The payload is already a compressed ZIP. Recompressing it makes the local
; one-click package much slower to build without materially reducing its size.
SetCompress off
AutoCloseWindow true
ShowInstDetails show

!include MUI2.nsh
!include LogicLib.nsh
!addplugindir .\script\

!define MUI_ICON "res\Quattro.ico"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}.${APP_VERSION_BUILD}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Quattro"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Quattro one-click update"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Quattro / Quattro contributors"

Function .onInit
  StrCpy $INSTDIR "$LOCALAPPDATA\Quattro"
  ${IfNot} ${FileExists} "$INSTDIR\Quattro.exe"
  ${AndIfNot} ${FileExists} "$INSTDIR\MintVPN.exe"
    MessageBox MB_OK|MB_ICONSTOP \
      "Текущая установка Quattro не найдена:$\r$\n$INSTDIR$\r$\n$\r$\nСначала установите Quattro обычным установщиком."
    Abort
  ${EndIf}

  MessageBox MB_YESNO|MB_ICONQUESTION \
    "Обновить текущую установку Quattro до ${APP_VERSION}?$\r$\n$\r$\nПеред обновлением закройте MintVPN и Quattro вручную.$\r$\nПодписка, профили и настройки сохранятся." \
    IDYES proceed
  Abort
  proceed:
FunctionEnd

!macro RequireClientsStopped
  ; Fail closed: Windows process enumeration completeness is not yet proven.
  MessageBox MB_OK|MB_ICONSTOP "Installation is disabled until process visibility and safe recovery are verified."
  Abort
  nsExec::ExecToStack '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -EncodedCommand dAByAHkAIAB7ACAAJABwACAAPQAgAEcAZQB0AC0AUAByAG8AYwBlAHMAcwAgAC0ARQByAHIAbwByAEEAYwB0AGkAbwBuACAAUwB0AG8AcAAgAHwAIABXAGgAZQByAGUALQBPAGIAagBlAGMAdAAgAHsAIAAkAF8ALgBQAHIAbwBjAGUAcwBzAE4AYQBtAGUAIAAtAGkAbgAgAEAAKAAnAFEAdQBhAHQAdAByAG8AJwAsACcAUQB1AGEAdAB0AHIAbwBDAG8AcgBlACcALAAnAE0AaQBuAHQAVgBQAE4AJwAsACcATQBpAG4AdABWAFAATgBDAG8AcgBlACcALAAnAFEAdQBhAHQAdAByAG8AVQBwAGQAYQB0AGUAcgAnACkAIAB9ADsAIABpAGYAIAAoACQAcAApACAAewAgAGUAeABpAHQAIAAxACAAfQA7ACAAZQB4AGkAdAAgADAAIAB9ACAAYwBhAHQAYwBoACAAewAgAGUAeABpAHQAIAAyACAAfQA='
  Pop $R0
  Pop $R1
  ${If} $R0 != 0
    MessageBox MB_OK|MB_ICONSTOP "Close MintVPN and Quattro, then retry. If process checking is unavailable, installation is stopped."
    Abort
  ${EndIf}
!macroend

Section "Update"
  !insertmacro RequireClientsStopped

  DetailPrint "Подготавливаю проверенный пакет обновления..."
  SetOutPath "$INSTDIR"
  SetOverwrite on
  File /oname=Quattro.zip "${UPDATE_ARCHIVE}"

  ; Run the worker outside the installation tree so it can atomically replace
  ; QuattroUpdater.exe along with the rest of the application.
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  File /oname=QuattroUpdateWorker.exe "deployment\windows-amd64\QuattroUpdater.exe"

  DetailPrint "Устанавливаю обновление без изменения пользовательских данных..."
  SetOutPath "$INSTDIR"
  ExecWait '"$PLUGINSDIR\QuattroUpdateWorker.exe"' $0
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONSTOP \
      "Обновление завершилось с ошибкой ($0).$\r$\nПодробности: $INSTDIR\QuattroUpdateError.log"
    Abort
  ${EndIf}

  WriteRegStr HKCU "Software\Quattro" "InstallPath" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quattro" \
              "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quattro" \
              "InstallLocation" "$INSTDIR"
  DetailPrint "Готово. Quattro перезапущен."
SectionEnd
