#!/bin/sh
#
# Build a small macOS launcher for the Win2VNC mode of vncviewer.
#
# It asks for the address of the remote system and for which side of
# the local screen it sits at, remembers both for next time, and starts
# the viewer with the options that this pair of client and server needs.
#
# Usage:
#   contrib/macos/make-win2vnc-app.sh [path-to-vncviewer]
#
# With no argument it looks for the viewer in a "build" directory next
# to this checkout.

set -e

SRCDIR=$(cd "$(dirname "$0")/../.." && pwd)
VIEWER=${1:-$SRCDIR/build/vncviewer/vncviewer}
APPDIR=$HOME/Applications
APP=$APPDIR/Win2VNC.app

if [ ! -x "$VIEWER" ]; then
	echo "No vncviewer at $VIEWER" >&2
	echo "Pass the path to it as the first argument." >&2
	exit 1
fi

SCRIPT=$(mktemp -t win2vnc)
trap 'rm -f "$SCRIPT"' EXIT

cat > "$SCRIPT" <<APPLESCRIPT
on run
	set viewer to "$VIEWER"
	set homePath to POSIX path of (path to home folder)
	set supportDir to homePath & "Library/Application Support"
	set hostFile to supportDir & "/win2vnc-host.txt"
	set edgeFile to supportDir & "/win2vnc-edge.txt"
	set logFile to homePath & "Library/Logs/win2vnc.log"

	try
		do shell script "test -x " & quoted form of viewer
	on error
		display dialog "No encuentro el visor en:" & linefeed & linefeed & viewer with title "Win2VNC" buttons {"OK"} default button "OK" with icon stop
		return
	end try

	set lastHost to ""
	try
		set lastHost to do shell script "cat " & quoted form of hostFile
	end try

	set theReply to display dialog "Direccion del equipo remoto (IP, o IP:puerto):" with title "Win2VNC" default answer lastHost buttons {"Cancelar", "Continuar"} default button "Continuar"
	if button returned of theReply is "Cancelar" then return
	set theHost to text returned of theReply
	if theHost is "" then return

	set lastEdge to "Derecha"
	try
		set lastEdge to do shell script "cat " & quoted form of edgeFile
	end try

	set edgeChoice to choose from list {"Derecha", "Izquierda", "Arriba", "Abajo"} with title "Win2VNC" with prompt "Donde esta ubicada la pantalla remota respecto de tu Mac:" default items {lastEdge} without multiple selections allowed
	if edgeChoice is false then return
	set edgeName to item 1 of edgeChoice

	if edgeName is "Izquierda" then
		set edgeParam to "Left"
	else if edgeName is "Arriba" then
		set edgeParam to "Top"
	else if edgeName is "Abajo" then
		set edgeParam to "Bottom"
	else
		set edgeParam to "Right"
	end if

	do shell script "mkdir -p " & quoted form of supportDir
	do shell script "printf '%s' " & quoted form of theHost & " > " & quoted form of hostFile
	do shell script "printf '%s' " & quoted form of edgeName & " > " & quoted form of edgeFile

	set cmd to quoted form of viewer
	set cmd to cmd & " -Win2VNCMode=1"
	set cmd to cmd & " -Win2VNCEdge=" & edgeParam
	set cmd to cmd & " -Win2VNCWidth=3"
	set cmd to cmd & " -AutoSelect=0"
	set cmd to cmd & " -ExtendedClipboard=0"
	set cmd to cmd & " -Log '*:stderr:30,Win2VNC:stderr:100'"
	set cmd to cmd & " " & quoted form of theHost
	set cmd to cmd & " 2>&1 | tee " & quoted form of logFile

	-- Going through Terminal rather than starting the viewer directly:
	-- a freshly built .app has not been granted Local Network access,
	-- and without it every connection fails with "No route to host",
	-- which looks exactly like the remote system being unreachable.
	tell application "Terminal"
		activate
		do script cmd
	end tell

	-- An application cannot bring itself to the front from the
	-- background on current macOS, so the password dialog would come up
	-- behind the terminal and the password would be typed in there, in
	-- the clear. System Events is allowed to do it on our behalf.
	repeat 60 times
		delay 0.25
		try
			tell application "System Events"
				if exists (first process whose name is "vncviewer") then
					set frontmost of (first process whose name is "vncviewer") to true
					exit repeat
				end if
			end tell
		end try
	end repeat

	delay 1
	try
		tell application "System Events" to set frontmost of (first process whose name is "vncviewer") to true
	end try
end run
APPLESCRIPT

mkdir -p "$APPDIR"
rm -rf "$APP"
osacompile -o "$APP" "$SCRIPT"

echo "Built $APP"
echo "Viewer: $VIEWER"
