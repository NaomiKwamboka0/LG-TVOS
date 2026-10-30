; Family TV installer (NSIS 3). Built by "make win-setup" with:
;   -DSRC=<folder from make win> -DOUTFILE=<Setup.exe> -DVERSION=<x.y.z> -DICON=<familytv.ico>
; Installs for the current user only (no administrator needed), adds Start
; menu and desktop shortcuts and an entry in Windows' "Apps" list. The
; family's settings, PIN and saves live in %APPDATA%\FamilyTV and are kept
; when Family TV is uninstalled or updated.
Unicode true
SetCompressor lzma ; per file: solid mode needs the whole 250 MB in memory at once

!include "MUI2.nsh"

Name "Family TV"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\FamilyTV"
RequestExecutionLevel user

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_WELCOMEPAGE_TITLE "Family TV ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "Kenyan TV channels, world news and live sports from the stations' own YouTube streams, plus games and parent controls.$\r$\n$\r$\nLive channels need an internet connection."
!define MUI_FINISHPAGE_RUN "$INSTDIR\FamilyTV.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start Family TV"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SRC}/LICENSES/NOTICE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!define UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\FamilyTV"

Section "Family TV"
    SetOutPath "$INSTDIR"
    File /r "${SRC}/*.*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateShortcut "$SMPROGRAMS\Family TV.lnk" "$INSTDIR\FamilyTV.exe"
    CreateShortcut "$DESKTOP\Family TV.lnk" "$INSTDIR\FamilyTV.exe"
    WriteRegStr HKCU "${UNINST}" "DisplayName" "Family TV"
    WriteRegStr HKCU "${UNINST}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "${UNINST}" "Publisher" "Family TV contributors"
    WriteRegStr HKCU "${UNINST}" "DisplayIcon" "$INSTDIR\FamilyTV.exe"
    WriteRegStr HKCU "${UNINST}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINST}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKCU "${UNINST}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINST}" "NoRepair" 1
SectionEnd

Section "Uninstall"
    Delete "$SMPROGRAMS\Family TV.lnk"
    Delete "$DESKTOP\Family TV.lnk"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKCU "${UNINST}"
SectionEnd
