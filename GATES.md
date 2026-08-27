# Gates: Native Windows Rewrite of NetworkMonitorLite

OWNS: native/**, scripts/**, GATES.md

Scope: Complete, dependency-free native Windows x64 rewrite of NetworkMonitorLite with full behavioral parity, robust network sampling, tray icon, taskbar widget, settings UI, and DPI awareness.

- [x] G1: Native x64 Release build succeeds with static CRT (/MT) producing NetworkMonitorLite.exe (Automated Build Test)
  CHECK: node scripts/verify-build.mjs
  EXPECT: build verification passed: NetworkMonitorLite.exe created with static CRT
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=6d68636e13438ec45fb98a4edc2c505bc74aa72221c967d6534acbd30f7249e3; output-bytes=74

- [x] G2: Compiler warnings are zero under strict flags (/W4 /WX /permissive- /std:c++20) (Automated Compiler Test)
  CHECK: node scripts/verify-warnings.mjs
  EXPECT: warning verification passed: zero compiler warnings under W4 WX
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=fe9a430f0e657b6140c10dd0dffa765ea791e4fc7db94097c0bd6e2fe44ce2fc; output-bytes=64

- [x] G3: Network interface enumeration correctly queries and filters physical and VPN interfaces with Gigabit NDIS 117 (Automated Runtime & Enum Test)
  CHECK: node scripts/verify-network-enum.mjs
  EXPECT: network enumeration verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=69c1b39bcbd9e00449e510ba6bd5b87b64c84d7fddc30d566c60710e10615253; output-bytes=40

- [x] G4: Adapter selection uses stable interface identity (NET_LUID) surviving interface index shifts (Automated Runtime & Mock Test)
  CHECK: node scripts/verify-stable-identity.mjs
  EXPECT: stable identity verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=caf61816de451ba040d6a96ea35770accc5edc6c93ea290412de676999f06e7e; output-bytes=36

- [x] G5: 64-bit RX and TX counters correctly accumulate and handle full 64-bit octet values (Automated Runtime Math Test)
  CHECK: node scripts/verify-counters-64bit.mjs
  EXPECT: 64-bit counters verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=39292d3747a74e4461ceabf3143591f0f8d6cadb545b5f7e9f1c30d1d97893f1; output-bytes=36

- [x] G6: Monotonic clock (QPC) measures actual elapsed time for throughput calculation without assuming fixed 1000ms ticks (Automated Runtime Clock Test)
  CHECK: node scripts/verify-monotonic-timing.mjs
  EXPECT: monotonic timing verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=0a23527abf78ed5cdf00a94db5e69e7778798d581b0d8252b5292ad306051d72; output-bytes=37

- [x] G7: Counter reset, adapter down, and adapter reconnect events are handled gracefully without invalid negative spikes or crashes (Automated Runtime Mock Test)
  CHECK: node scripts/verify-counter-reset.mjs
  EXPECT: counter reset and adapter recovery verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=78a01a5b413f201bbbafac539d6918060c937069d46ffa1353af5d03b1f6fd61; output-bytes=55

- [x] G8: Main window layout, controls, dark theme, speed formatters, and author link match C# specifications (Automated Formatting Test & Static Control Inspection; visual layout subject to interactive manual check)
  CHECK: node scripts/verify-main-window.mjs
  EXPECT: main window verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=f5ff6a13d47b64102e3df70ec4ce4f93ae886dfee5f4317979be54332c20094b; output-bytes=32

- [x] G9: Tray icon renders dynamic speed text and context menu provides Show Window, Settings, Show Taskbar Widget, and Exit (Automated Static & Format Inspection; live tray animation subject to interactive manual check)
  CHECK: node scripts/verify-tray.mjs
  EXPECT: tray icon and menu verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=5b3b294ae615e49f42221eb2cc9844f4f974c4bca9d342dad0425782df38e08a; output-bytes=39

- [x] G10: Taskbar overlay widget supports monitor-aware positioning, drag, topmost, opacity, border, and dynamic settings (Automated Static Geometry Inspection; multi-monitor docking subject to interactive manual check)
  CHECK: node scripts/verify-taskbar-widget.mjs
  EXPECT: taskbar widget verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=d3d71055005f3c855163f60a221058d05a89f447926cbd722862b1bb408e44d8; output-bytes=35

- [x] G11: Settings persistence accurately loads and saves INI configuration under %APPDATA%\NetworkMonitorLite\settings.ini (Automated Runtime Roundtrip Test)
  CHECK: node scripts/verify-settings-persistence.mjs
  EXPECT: settings persistence verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=37f713957cafba604d82beb87babc0b820d860726c98d832a8f4a9a36e71e94f; output-bytes=41

- [x] G12: Native Settings UI allows choosing colors, fonts with all FontStyle bits, and live preview (Automated Static Inspection; modal dialog interaction subject to interactive manual check)
  CHECK: node scripts/verify-settings-ui.mjs
  EXPECT: settings UI verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=bf2f27a0eefce7f17efea619552c4032e068443a7a7a4c125ab4668247e7ce37; output-bytes=32

- [x] G13: Tray Exit executes clean shutdown terminating timers, windows, tray icon, and propagating WM_QUIT cleanly (Automated Static Loop Inspection)
  CHECK: node scripts/verify-shutdown.mjs
  EXPECT: shutdown verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=ac2af9eceb5e7baad306e387416cbb278b243e5e48ff33dd395b93f7f5ecdf7b; output-bytes=29

- [x] G14: Explorer restart is handled via TaskbarCreated window message to restore the tray icon without leaks (Automated Static & Handle Inspection)
  CHECK: node scripts/verify-explorer-restart.mjs
  EXPECT: explorer restart handling verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=ed32ff16c5da9cb9c5462930c300f70b4562c369eb646ff37eddc3025d5fd1c6; output-bytes=46

- [x] G15: Per-Monitor DPI Awareness V2 is initialized with safe font reallocation and control repositioning (Automated Static & DPI API Inspection; dynamic multi-DPI monitor dragging subject to interactive manual check)
  CHECK: node scripts/verify-dpi.mjs
  EXPECT: DPI awareness verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=11a19be659bc8f93f7915cdabcd53f0d257a99a72cbfd867b53a6ca34233446c; output-bytes=34

- [x] G16: Native GDI and Win32 handles (HICON, HFONT, HBRUSH, HDC, HMENU) have explicit lifetime management with zero resource leaks (Automated Runtime Stress Test via GetGuiResources)
  CHECK: node scripts/verify-resource-lifetimes.mjs
  EXPECT: resource lifetime verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=3b687eb6cf34bd0a71c6bd194335187e8c5e19c1a06394169902f0d979564a6f; output-bytes=38

- [x] G17: PE imports inspection confirms zero VC runtime DLLs, zero .NET runtime, and only standard Windows system DLLs (Automated PE Dumpbin Test)
  CHECK: node scripts/verify-pe-imports.mjs
  EXPECT: PE imports verification passed: zero external runtime DLLs
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=f76b5b26dc67fd535b99ab4cba654bdd5a18a23f22673ed7fab769bb0f8b2f8c; output-bytes=59

- [x] G18: Security and privacy audit confirms zero telemetry, zero updaters, zero command/process execution, and zero unsolicited shell activity (explicit user-clicked ShellExecuteW to open project GitHub URL is sole allowed exception)
  CHECK: node scripts/verify-security-privacy.mjs
  EXPECT: security and privacy verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=c9b9c66c013e6c4bf156faafa2d0361b6edd55347c5395f442ddcd2cfeb8cd6c; output-bytes=41

- [x] G19: Outbound network audit confirms zero unsolicited socket/HTTP connections (Automated Source & Dependency Audit)
  CHECK: node scripts/verify-network-traffic.mjs
  EXPECT: network traffic audit verification passed: zero unsolicited network connections
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=e48d23ec4187ec70e70d1ebec41b94690294ae197f5218722c12fe790f9e13ff; output-bytes=80

- [x] G20: Repository cleanliness confirms obsolete Qwen test/debug artifacts are removed and build products are cleanly located in out/ (Automated Filesystem Audit)
  CHECK: node scripts/verify-cleanliness.mjs
  EXPECT: repository cleanliness verification passed
  EVIDENCE: exit=0; shell=C:\WINDOWS\system32\cmd.exe; cwd=C:\Users\Sheriff\Desktop\src\networkMonitorLite; path=751bdd565e21/18 entries; EXPECT=matched; output-sha256=5619e91e76524d59d2d8cd037631cdf3a6cbf9f3dc36ebbf393589e68583e961; output-bytes=43
