from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
(root / "build").mkdir(exist_ok=True)
version = (root / "VERSION").read_text(encoding="utf-8").strip()
match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:-rc\.(\d+))?", version)
if not match:
    raise SystemExit(f"Unsupported VERSION format for Windows resource: {version!r}")

major, minor, patch = (int(match.group(i)) for i in (1, 2, 3))
prerelease = int(match.group(4) or 0)
flags = "0x2L" if "-" in version else "0x0L"

script = "window.__floggfyNative=true;\n" + (root / "native/metadata_collector.js").read_text(encoding="utf-8")
(root / "build/metadata_script.h").write_text(
    'static constexpr wchar_t metadata_script[]=LR"FLOGGFY(' + script + ')FLOGGFY";\n',
    encoding="utf-8",
)
ui_files = [
    "native/ui/classic_core.js",
    "native/ui/classic_settings.js",
    "native/ui/classic_status.js",
    "native/ui/classic_recovery.js",
    "native/ui/classic_canvas.js",
    "native/ui/classic_m3u.js",
    "native/ui/classic_context.js",
    "native/ui/classic_boot.js",
]
ui_script = "\n".join((root / path).read_text(encoding="utf-8") for path in ui_files)
(root / "build/soggfy_ui_script.h").write_text(
    'static constexpr wchar_t soggfy_ui_script[]=LR"SOGGFYUI(' + ui_script + ')SOGGFYUI";\n',
    encoding="utf-8",
)

resource = f'''#include <winver.h>

VS_VERSION_INFO VERSIONINFO
 FILEVERSION {major},{minor},{patch},{prerelease}
 PRODUCTVERSION {major},{minor},{patch},{prerelease}
 FILEFLAGSMASK 0x3fL
 FILEFLAGS {flags}
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_DLL
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "Comments", "Modern x64 Soggfy continuation; Floggfy-derived capture core"
            VALUE "FileDescription", "Soggfy Spotify history capture proxy"
            VALUE "FileVersion", "{version}"
            VALUE "InternalName", "Soggfy"
            VALUE "LegalCopyright", "MIT / CC0; see bundled licenses"
            VALUE "OriginalFilename", "version.dll"
            VALUE "ProductName", "Soggfy"
            VALUE "ProductVersion", "{version}"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 1200
    END
END
'''
(root / "build/version.rc").write_text(resource, encoding="utf-8")
