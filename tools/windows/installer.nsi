; Instalador de UltimateTracker para Windows (NSIS 3).
; Se instala solo para el usuario (no pide permisos de administrador) en
; %LOCALAPPDATA%\Programs\UltimateTracker, con accesos directos en el escritorio
; y en el menú Inicio, y aparece en "Aplicaciones instaladas" para desinstalarla.
;
; Compilar:  makensis -DAPPDIR=<carpeta con UltimateTracker.exe y sus DLL> -DVERSION=1.0.0 -DVERSION_LABEL="1.0 beta 1" installer.nsi
; (makensis existe para Windows y para Linux.)

Unicode true
SetCompressor /SOLID lzma

!ifndef APPDIR
  !error "Falta -DAPPDIR=<carpeta de la app compilada>"
!endif
!ifndef VERSION
  !define VERSION "1.0.0"
!endif
!ifndef VERSION_LABEL
  !define VERSION_LABEL "${VERSION}"
!endif
!ifndef OUTFILE
  !define OUTFILE "UltimateTracker-setup.exe"
!endif

!define APPNAME   "UltimateTracker"
!define PUBLISHER "Luca Salviani"
!define EXE       "UltimateTracker.exe"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
!define RUNKEY    "Software\Microsoft\Windows\CurrentVersion\Run"

Name "${APPNAME}"
Caption "Instalar ${APPNAME} ${VERSION_LABEL}"
BrandingText "${APPNAME} ${VERSION_LABEL}"
OutFile "${OUTFILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\${APPNAME}"
InstallDirRegKey HKCU "${UNINSTKEY}" "InstallLocation"
ManifestDPIAware true

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "ProductVersion" "${VERSION_LABEL}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "Instalador de ${APPNAME}"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
VIAddVersionKey "LegalCopyright" "© ${PUBLISHER}"

!include "MUI2.nsh"
!include "LogicLib.nsh"

!define MUI_ICON   "..\..\resources\app.ico"
!define MUI_UNICON "..\..\resources\app.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Bienvenido a ${APPNAME}"
!define MUI_WELCOMEPAGE_TEXT "Pomodoros y horas de estudio en una ventana chica, con una grilla de cuadraditos y etapas camino a las 10.000 horas.$\r$\n$\r$\nSe instala solo para tu usuario y no necesita permisos de administrador.$\r$\n$\r$\nVersión ${VERSION_LABEL}."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir ${APPNAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Spanish"

; La app no se puede reemplazar ni borrar mientras está abierta.
!macro WAIT_APP_CLOSED
  ${Do}
    nsExec::ExecToStack 'cmd /c tasklist /FI "IMAGENAME eq ${EXE}" /NH | find /I "${EXE}"'
    Pop $0
    Pop $1
    ${If} $0 != 0
      ${Break}
    ${EndIf}
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "${APPNAME} está abierta.$\r$\nCerrala (menú ⋯ → Salir, o clic derecho en el ícono de la bandeja → Salir) y tocá Reintentar." IDRETRY +2
    Abort
  ${Loop}
!macroend

Section "Instalar"
  !insertmacro WAIT_APP_CLOSED
  SetOutPath "$INSTDIR"
  File /r "${APPDIR}\*.*"
  WriteUninstaller "$INSTDIR\Desinstalar.exe"

  CreateShortcut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0

  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION_LABEL}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\${EXE}"
  WriteRegStr HKCU "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Desinstalar.exe"'
  WriteRegStr HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Desinstalar.exe" /S'
  WriteRegStr HKCU "${UNINSTKEY}" "URLInfoAbout" "https://github.com/LucaSalviani/UltimateTracker"
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1
  ; Tamaño aproximado para "Aplicaciones instaladas" (KB)
  WriteRegDWORD HKCU "${UNINSTKEY}" "EstimatedSize" 85000
SectionEnd

Section "Uninstall"
  !insertmacro WAIT_APP_CLOSED
  ; Que no quede arrancando con Windows
  DeleteRegValue HKCU "${RUNKEY}" "${APPNAME}"
  Delete "$SMPROGRAMS\${APPNAME}.lnk"
  Delete "$DESKTOP\${APPNAME}.lnk"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "${UNINSTKEY}"
  ; Los datos (horas, actividades, ajustes) solo se borran si lo pedís.
  IfSilent done
  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "¿Borrar también tus datos (horas, actividades y ajustes)?$\r$\n$\r$\nSi elegís No, quedan guardados para cuando la vuelvas a instalar." IDNO done
  RMDir /r "$APPDATA\${APPNAME}"
  done:
SectionEnd
