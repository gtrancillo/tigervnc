/* Copyright 2026 TigerVNC contributors
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifndef __WIN2VNC_H__
#define __WIN2VNC_H__

#include <stdint.h>

#include <FL/Fl_Window.H>

class Viewport;

// Where the remote system is located relative to the local screen
enum Win2VNCEdge {
  Win2VNCRight,
  Win2VNCLeft,
  Win2VNCTop,
  Win2VNCBottom,
};

// Current value of the Win2VNCEdge parameter
Win2VNCEdge win2vncEdgeParam();

// Move the local pointer to the given screen coordinates
void win2vncWarpPointer(int x, int y);

// A window covering the entire local screen, used to capture all local
// pointer motion whilst the remote system has control of the local
// input devices.
//
// The trigger window (DesktopWindow) is just a thin strip along one of
// the screen edges, which means it only sees pointer motion inside that
// strip. That is enough to notice that the user wants to move over to
// the remote system, but not to actually follow the pointer afterwards.
// Hence this window, which is put up (invisible, and with the local
// cursor hidden) as soon as the strip is touched. The local pointer is
// warped to the opposite side of the screen and every motion is then
// translated in to an absolute position on the remote screen.

class Win2VNCOverlay : public Fl_Window {
public:
  Win2VNCOverlay(Viewport* viewport_);
  ~Win2VNCOverlay();

  bool active() const { return controlling; }

  // Take control of the local input devices. (entryX, entryY) is where
  // the pointer touched the trigger strip.
  void takeControl(int entryX, int entryY);

  // Give the local input devices back to the local system
  void releaseControl();

  int handle(int event) override;
  void draw() override;

private:
  void handleMouse(int event);
  void sendPointer(uint16_t buttonMask);
  bool escaped() const;
  void clampRemotePos();
  void warpTo(int x, int y);
  void recenterPointer();
  void hideLocalCursor();
  void restoreLocalCursor();

private:
  Viewport* viewport;

  bool controlling;

  Win2VNCEdge edge;

  // The local screen we are covering
  int screenX, screenY, screenW, screenH;

  // The usable part of that screen, i.e. where the trigger strip is
  int workX, workY, workW, workH;

  // Size of the remote screen
  int remoteW, remoteH;

  // Where the pointer currently is on the remote screen. This is
  // allowed to go slightly outside of the remote screen, as that is how
  // we detect that the user wants to go back to the local system.
  int remoteX, remoteY;

  // Last seen position of the local pointer
  int lastX, lastY;

  bool cursorHidden;

  // Time (in seconds) when we took control, used to ignore bogus focus
  // events right after showing this window
  double startTime;
};

#endif
