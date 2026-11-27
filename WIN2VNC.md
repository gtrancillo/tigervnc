# Win2VNC mode

This fork adds a Win2VNC mode to the TigerVNC viewer: the remote screen
behaves as an extra monitor placed next to the local one, driven by the
local mouse and keyboard. It is developed against **droidVNC-NG** on an
Android tablet, from **macOS**, but nothing in it is macOS only.

Everything here is also in `vncviewer/vncviewer.man` and in the Win2VNC
page of the viewer's Options dialog. This file adds the parts that only
make sense for this particular setup.

## How it works

Turn it on and the viewer window shrinks to a thin strip along one edge
of your screen. Move the pointer on to that strip and the local mouse
and keyboard are handed over to the remote session. To come back, push
the pointer out through the *opposite* edge of the remote screen, or
press and release the shortcut modifiers (Ctrl+Alt by default).

The strip alone cannot follow the pointer — it only sees what happens
inside itself. So touching it puts up a transparent window covering
every screen you have, which hides the local cursor and turns each
movement into an absolute position on the remote screen.

### Parameters

| Parameter | Meaning | Default |
| --- | --- | --- |
| `Win2VNCMode` | Turn the mode on | off |
| `Win2VNCEdge` | Where the remote system is: `Right`, `Left`, `Top`, `Bottom` | `Right` |
| `Win2VNCWidth` | Thickness of the trigger strip, in pixels | 3 |

## Building on macOS

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

Debug builds add `-Werror`. If one fails in `H264DecoderContext.cxx`,
that is unrelated to this work: the H.264 sources are built even when
H.264 support was not found. Build Release, or configure with
`-DENABLE_H264=OFF`.

## The macOS launcher

```sh
contrib/macos/make-win2vnc-app.sh [path-to-vncviewer]
```

This builds `~/Applications/Win2VNC.app`. It asks for the address of
the remote system and for which side of your screen it sits at,
remembers both for next time, and starts the viewer with the options
below already set. The log goes to `~/Library/Logs/win2vnc.log`.

With no argument it looks for the viewer in `build/vncviewer/vncviewer`
next to the checkout. Pass the path explicitly if you build elsewhere.

Drag the app to the Dock to keep it to hand. The first time macOS may
refuse to open an unsigned app: right click, *Open*, then *Open* again
in the dialog. None of this needs an administrator.

### Options the launcher sets, and why

**`-AutoSelect=0`.** With auto select on, the viewer changes the pixel
format in the middle of the session whenever its estimate of the
available bandwidth crosses 256 kbps. There is no hysteresis, so an
estimate that hovers around that figure flips the format over and over.
droidVNC-NG does not support the `fence` extension and so cannot
synchronise such a change: rectangles already in flight are decoded with
the wrong number of bytes per pixel, the stream loses its place, and the
connection dies with `Invalid rectangle received`.

The real fix would be to extend the guard that already exists in
`CConn::updatePixelFormat()`, which today only applies to servers older
than RFB 3.8. droidVNC-NG announces 3.8, so it is skipped. Not done yet.

**`-ExtendedClipboard=0`.** The extended clipboard sends its text with a
terminating null character, as the format requires. droidVNC-NG takes
the length at face value and passes that byte on, so everything pasted
on the tablet ends with an invisible extra character — which makes
pasted passwords fail with nothing to see in the field. Turning the
extension off falls back to the original clipboard messages, which carry
no terminator.

The cost is real: those older messages are Latin-1 only. Text outside
Latin-1, emoji among it, will not survive a copy and paste.

**`-Log '*:stderr:30,Win2VNC:stderr:100'`.** Keeps this mode's own
messages at debug level and everything else at the usual one. Logging
everything at debug prints a line for every key you press.

### Why it goes through Terminal

The launcher starts the viewer inside a Terminal window instead of
running it directly. That is deliberate.

Since Sonoma, macOS asks each application for permission to reach the
local network. A freshly built app has never been granted it, and while
it is denied the system returns `No route to host (65)` — the same error
as a machine that is not there at all, which makes it look like the
address is wrong. Terminal was granted that permission long ago, so the
viewer inherits it.

It also means the log is visible while you work.

### The password dialog and focus

An application cannot bring itself to the front from the background on
current macOS, however it asks; Apple has closed that off. So the
password dialog would come up behind the terminal and the password would
be typed into *that*, in the clear.

The launcher works around it by asking System Events, which the system
does allow to do this, to bring the viewer forward. You will be asked to
approve that once. It is Automation, not Accessibility, and needs no
administrator.

The supported way out would be a real `.app` bundle, built with
`release/makemacapp`: an application launched through LaunchServices
gets the keyboard by right. That means rebuilding the bundle on every
compile and granting Local Network access again, which is why the
launcher does not do it.

## Things to know about this setup

**Pick an edge that is a real edge.** If you choose a side where a
second physical monitor sits, every trip to that monitor will enter the
remote session. Nothing can tell the two intentions apart. The bottom
edge is usually taken by the Dock.

**The connection is not encrypted.** droidVNC-NG offers only `VncAuth`,
which truncates the password to eight characters and leaves everything
after authentication — the screen, your keystrokes, the clipboard — in
the clear on the network. That includes any password you type *through*
the session. The viewer supports TLS and X509, but they cannot be used
against a server that does not offer them, and no Android VNC server
appears to.

If that matters, put the whole thing inside a VPN between the two
devices. Tailscale or WireGuard both have macOS and Android clients;
point the viewer at the VPN address instead. As a side effect it also
gets you past Wi-Fi networks that isolate clients from each other.

## Proposing this upstream

Three branches are kept rebased on the upstream commit this fork started
from, each holding one self-contained change, so they can be offered to
`tigervnc/tigervnc` separately:

| Branch | Contents |
| --- | --- |
| `upstream/win2vnc` | The Win2VNC mode |
| `upstream/extended-clipboard` | The `ExtendedClipboard` parameter |
| `upstream/auth-focus` | Focus the first field of the authentication dialog |

They deliberately leave out everything specific to this fork: the
relaxed FLTK version check in the top level `CMakeLists.txt`, this file,
and `contrib/macos`.
