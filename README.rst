About TigerVNC
==============

Virtual Network Computing (VNC) is a remote display system which allows you to
view and interact with a virtual desktop environment that is running on another
computer on the network.  Using VNC, you can run graphical applications on a
remote machine and send only the display from these applications to your local
machine.  VNC is platform-independent and supports a wide variety of operating
systems and architectures as both servers and clients.

TigerVNC is a high-speed version of VNC based on the RealVNC 4 and X.org code
bases.  TigerVNC started as a next-generation development effort for TightVNC
on Unix and Linux platforms, but it split from its parent project in early 2009
so that TightVNC could focus on Windows platforms.  TigerVNC supports a variant
of Tight encoding that is greatly accelerated by the use of the libjpeg-turbo
JPEG codec.


Legal
=====

Incomplete and generally out of date copyright list::

        Copyright (C) 1999 AT&T Laboratories Cambridge
        Copyright (C) 2002-2005 RealVNC Ltd.
        Copyright (C) 2000-2006 TightVNC Group
        Copyright (C) 2005-2006 Martin Koegler
        Copyright (C) 2005-2006 Sun Microsystems, Inc.
        Copyright (C) 2006 OCCAM Financial Technology
        Copyright (C) 2000-2008 Constantin Kaplinsky
        Copyright (C) 2004-2017 Peter Astrand for Cendio AB
        Copyright (C) 2010 Antoine Martin
        Copyright (C) 2010 m-privacy GmbH
        Copyright (C) 2009-2011 D. R. Commander
        Copyright (C) 2009-2011 Pierre Ossman for Cendio AB
        Copyright (C) 2004, 2009-2011 Red Hat, Inc.
        Copyright (C) 2009-2026 TigerVNC team
        All Rights Reserved.

This software is distributed under the GNU General Public Licence as published
by the Free Software Foundation.  See the file LICENCE.TXT for the conditions
under which this software is made available.  TigerVNC also contains code from
other sources.  See the Acknowledgements section below, and the individual
source files, for details of the conditions under which they are made
available.


All Platforms
=============

All versions of TigerVNC contain the following programs:

* vncviewer - the cross-platform TigerVNC Viewer, written using FLTK.
              vncviewer connects to a VNC server and allows you to interact
              with the remote desktop being displayed by the VNC server.  The
              VNC server can be running on a Windows or a Unix/Linux machine.


Windows-specific
================

The Windows version of TigerVNC contains the following programs:

* winvnc - the TigerVNC server for Windows.  winvnc allows a Windows desktop to
           be accessed remotely using a VNC viewer.

WARNING: winvnc is currently unmaintained and and may not function correctly.

winvnc may not work if the Fast user switching or Remote desktop features are
in use.


Unix/Linux-specific (not Mac)
=============================

The Unix/Linux version of TigerVNC contains the following programs:

* Xvnc - the TigerVNC server for Unix.  Xvnc is both a VNC server and an X
         server with a "virtual" framebuffer.  You should normally use the
         vncserver service to start Xvnc.

* vncpasswd - a program which allows you to change the VNC password used to
              access your VNC server sessions (assuming that VNC authentication
              is being used.) This command must be run to set a password before
              using VNC authentication with any of the servers or services.

* vncconfig - a program which is used to configure and control a running
              instance of Xvnc.

* x0vncserver - an inefficient VNC server which continuously polls any X
                display, allowing it to be controlled via VNC.  It is intended
                mainly as a demonstration of a simple VNC server.

* w0vncserver - a VNC server for Wayland compositors which gives control
                over your local Wayland session.

It also contains the following systemd service:

* vncserver@.service - a service to start a user session with Xvnc and one of
                       the desktop environments available on the system.

Win2VNC mode (added by this fork)
=================================

This fork adds a Win2VNC mode to the viewer: the remote screen behaves as an
extra monitor placed next to the local one, driven by the local mouse and
keyboard.  It is developed against droidVNC-NG on an Android tablet, from
macOS, but nothing in it is macOS only.

What it does
------------

Turn it on and the viewer window shrinks to a thin strip along one edge of
your screen.  Move the pointer on to that strip and the local mouse and
keyboard are handed over to the remote session.  To come back, push the
pointer out through the *opposite* edge of the remote screen, or press and
release the shortcut modifiers (Ctrl+Alt by default).

The strip alone cannot follow the pointer, since it only sees what happens
inside itself.  So touching it puts up a transparent window covering every
screen you have, which hides the local cursor and turns each movement in to
an absolute position on the remote screen.  Covering every screen rather than
just the one the pointer came in on matters: it only takes one quick movement
to reach a neighbouring monitor, and out there nothing would be seen at all.

Parameters, also described in the vncviewer manual page and on the Win2VNC
page of the Options dialog:

* Win2VNCMode - turn the mode on.  Default is off.
* Win2VNCEdge - where the remote system is: Right, Left, Top or Bottom.
                Default is Right.
* Win2VNCWidth - thickness of the trigger strip, in pixels.  Default is 3.

Building on macOS
-----------------

::

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j4

Debug builds add -Werror.  If one fails in H264DecoderContext.cxx, that is
unrelated to this work: the H.264 sources are built even when H.264 support
was not found.  Build Release, or configure with -DENABLE_H264=OFF.

The macOS launcher
------------------

::

    contrib/macos/make-win2vnc-app.sh [path-to-vncviewer]

This builds ~/Applications/Win2VNC.app.  It asks for the address of the
remote system and for which side of your screen it sits at, remembers both
for next time, and starts the viewer with the options below already set.  The
log goes to ~/Library/Logs/win2vnc.log.

With no argument it looks for the viewer in build/vncviewer/vncviewer next to
the checkout.  Pass the path explicitly if you build elsewhere.

Drag the app to the Dock to keep it to hand.  The first time, macOS may
refuse to open an unsigned app: right click, Open, then Open again in the
dialog.  None of this needs an administrator.

Options the launcher sets, and why
----------------------------------

``-AutoSelect=0``
    With auto select on, the viewer changes the pixel format in the middle of
    the session whenever its estimate of the available bandwidth crosses
    256 kbps.  There is no hysteresis, so an estimate that hovers around that
    figure flips the format over and over.  droidVNC-NG does not support the
    fence extension and so cannot synchronise such a change: rectangles
    already in flight are decoded with the wrong number of bytes per pixel,
    the stream loses its place, and the connection dies with "Invalid
    rectangle received".

    The real fix would be to extend the guard that already exists in
    CConn::updatePixelFormat(), which today only applies to servers older
    than RFB 3.8.  droidVNC-NG announces 3.8, so it is skipped.  Not done yet.

``-ExtendedClipboard=0``
    The extended clipboard sends its text with a terminating null character,
    as the format requires.  droidVNC-NG takes the length at face value and
    passes that byte on, so everything pasted on the tablet ends with an
    invisible extra character, which makes pasted passwords fail with nothing
    to see in the field.  Turning the extension off falls back to the
    original clipboard messages, which carry no terminator.

    The cost is real: those older messages are Latin-1 only.  Text outside
    Latin-1, emoji among it, will not survive a copy and paste.

``-Log '*:stderr:30,Win2VNC:stderr:100'``
    Keeps this mode's own messages at debug level and everything else at the
    usual one.  Logging everything at debug prints a line for every key
    pressed.

Why the launcher goes through Terminal
--------------------------------------

The launcher starts the viewer inside a Terminal window instead of running it
directly.  That is deliberate.

Since Sonoma, macOS asks each application for permission to reach the local
network.  A freshly built app has never been granted it, and while it is
denied the system returns "No route to host (65)", the same error as a
machine that is not there at all, which makes it look as though the address
is wrong.  Terminal was granted that permission long ago, so the viewer
inherits it.

It also means the log is visible while you work.

The password dialog and focus
-----------------------------

An application cannot bring itself to the front from the background on
current macOS, however it asks.  So the password dialog would come up behind
the terminal and the password would be typed in to that, in the clear.

The launcher works around it by asking System Events, which the system does
allow to do this, to bring the viewer forward.  You will be asked to approve
that once.  It is Automation, not Accessibility, and needs no administrator.

The supported way out would be a real .app bundle, built with
release/makemacapp: an application launched through LaunchServices gets the
keyboard by right.  That means rebuilding the bundle on every compile and
granting Local Network access again, which is why the launcher does not do
it.

Things to know about this setup
-------------------------------

Pick an edge that is a real edge.  If you choose a side where a second
physical monitor sits, every trip to that monitor will enter the remote
session.  Nothing can tell the two intentions apart.  The bottom edge is
usually taken by the Dock.

The connection is not encrypted.  droidVNC-NG offers only VncAuth, which
truncates the password to eight characters and leaves everything after
authentication, the screen and keystrokes and clipboard alike, in the clear
on the network.  That includes any password typed *through* the session.  The
viewer supports TLS and X509, but they cannot be used against a server that
does not offer them, and no Android VNC server appears to.

If that matters, put the whole thing inside a VPN between the two devices.
Tailscale and WireGuard both have macOS and Android clients; point the viewer
at the VPN address instead.  As a side effect it also gets past Wi-Fi
networks that isolate clients from each other.

Proposing this upstream
-----------------------

The branch upstream/proposal holds these changes rebased on the upstream
commit this fork started from, as three self-contained commits: the Win2VNC
mode, the ExtendedClipboard parameter, and focusing the first field of the
authentication dialog.

It deliberately leaves out everything specific to this fork: the relaxed FLTK
version check in the top level CMakeLists.txt, this section, and contrib/macos.


ACKNOWLEDGEMENTS
================

This distribution contains public domain DES software by Richard Outerbridge.
This is:

    Copyright (c) 1988,1989,1990,1991,1992 by Richard Outerbridge.
    (GEnie : OUTER; CIS : [71755,204]) Graven Imagery, 1992.


This distribution contains software from the X Window System.  This is:

 Copyright 1987, 1988, 1998  The Open Group
 
 Permission to use, copy, modify, distribute, and sell this software and its
 documentation for any purpose is hereby granted without fee, provided that
 the above copyright notice appear in all copies and that both that
 copyright notice and this permission notice appear in supporting
 documentation.
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name of The Open Group shall not be
 used in advertising or otherwise to promote the sale, use or other dealings
 in this Software without prior written authorization from The Open Group.
 
 
 Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.
 
                         All Rights Reserved
 
 Permission to use, copy, modify, and distribute this software and its 
 documentation for any purpose and without fee is hereby granted, 
 provided that the above copyright notice appear in all copies and that
 both that copyright notice and this permission notice appear in 
 supporting documentation, and that the name of Digital not be
 used in advertising or publicity pertaining to distribution of the
 software without specific, written prior permission.  
 
 DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 SOFTWARE.
