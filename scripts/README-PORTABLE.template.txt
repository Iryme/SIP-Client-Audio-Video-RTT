SIP Client - Audio / Video / RTT -- Portable Windows Bundle
Version __VERSION__ (commit __GITCOMMIT__)
Qt __QTVERSION__ / PJSIP __PJVERSION__ / Release / x64

WHAT THIS IS
------------
A self-contained build. Everything SIPClient.exe needs (Qt DLLs and
plugins, the C++ runtime, and the media/codec libraries) is included in
this folder. Qt, Visual Studio, and PJSIP do NOT need to be installed on
the target machine.

REQUIREMENTS
------------
- Windows 10 or Windows 11, x64.
- No administrator privileges required.
- A working camera/microphone if you want to test audio/video calls.

HOW TO START
------------
1. Unzip this folder anywhere (Desktop, a USB drive, etc.) -- do not run
   it directly from inside the zip.
2. Double-click SIPClient.exe.
3. Windows may show a Firewall prompt the first time SIP/RTP traffic is
   sent -- allow it on whichever networks you intend to test with,
   otherwise registration/calls will silently fail to reach the network.

WHERE CONFIGURATION IS SAVED
-----------------------------
Nothing is written inside this folder. On first run the app creates its
own settings under the current Windows user's standard app-data location
(the Qt QSettings default, i.e. the registry under
HKEY_CURRENT_USER\Software\SIPClient on Windows). No SIP account,
password, or server address ships in this bundle -- you configure your
own profile from Settings after first launch.

MULTIPLE PROFILES / CONFIG DIRECTORY OVERRIDE
----------------------------------------------
To run more than one independent profile side by side (e.g. two test
accounts on the same machine), point each instance at its own directory
with either the command-line flag:

    SIPClient.exe --config-dir C:\Temp\sipclient-profile-a

or the equivalent environment variable (the flag wins if both are set):

    set SIPCLIENT_CONFIG_DIR=C:\Temp\sipclient-profile-a
    SIPClient.exe

Use a different directory per profile. If neither is set, the app falls
back to the default per-user location above.

CAMERA / MICROPHONE PERMISSIONS
--------------------------------
Windows may prompt for camera/microphone access the first time the app
tries to use them (Settings > Privacy & security > Camera/Microphone).
Grant access to see local video preview and to be heard on calls.

VISUAL C++ RUNTIME
-------------------
The required Microsoft Visual C++ runtime DLLs are already included next
to SIPClient.exe (no separate install needed for normal use). A copy of
the official vc_redist.x64.exe installer is also included in this folder
for environments that prefer a system-wide install instead -- run it only
if SIPClient.exe fails to start with a "missing VCRUNTIME140.dll" style
error.

COLLECTING DIAGNOSTICS
------------------------
Tools > Diagnostics Center > Export lets you save a diagnostics bundle
(a folder or .zip, depending on the Qt build) with version info, recent
logs, and call/session state -- URLs, credentials, and message bodies are
redacted before export. Attach that bundle when reporting an issue.

KNOWN LIMITATIONS
------------------
- Experimental MSRP relay / LMPE features are not enabled by default in
  this bundle.
- No packet capture is started automatically; this build does not
  request elevated privileges for anything.
- This is a portable Release build for testing -- there is no installer,
  no Start Menu entry, and no auto-update.

VERSION
-------
See version-info.json in this folder for the exact application, Qt, and
PJSIP versions and the git commit this bundle was built from.
