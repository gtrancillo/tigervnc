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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>

#include <algorithm>

#include <core/LogWriter.h>
#include <core/Rect.h>
#include <core/time.h>

#include <rfb/PixelFormat.h>

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/x.H>

#include "Win2VNC.h"
#include "Viewport.h"
#include "parameters.h"

#if defined(WIN32)
#include "win32.h"
#elif defined(__APPLE__)
#include "cocoa.h"
#else
#include "x11.h"
#include "touch.h"
#endif

static core::LogWriter vlog("Win2VNC");

// How close to the border of the local screen the pointer may get
// before we warp it back to the middle. Without this we would run out
// of local screen to move on long before we've crossed the remote
// screen.
static const int RECENTER_MARGIN = 16;

// How far outside of the remote screen the user has to push before we
// hand control back to the local system
static const int ESCAPE_MARGIN = 12;

// How far from the trigger strip the local pointer is placed when we
// give control back, so we don't immediately trigger again
static const int RETURN_GAP = 16;

// Ignore focus changes for this long after taking control (in ms)
static const unsigned FOCUS_GRACE_MS = 500;

Win2VNCEdge win2vncEdgeParam()
{
  if (win2vncEdge == "Left")
    return Win2VNCLeft;
  if (win2vncEdge == "Top")
    return Win2VNCTop;
  if (win2vncEdge == "Bottom")
    return Win2VNCBottom;
  return Win2VNCRight;
}

void win2vncWarpPointer(int x, int y)
{
#if defined(WIN32)
  SetCursorPos(x, y);
#elif defined(__APPLE__)
  cocoa_warp_cursor(x, y);
#else
  x11_warp_pointer(x, y);
#endif
}

static uint16_t currentButtonMask()
{
  uint16_t buttonMask;

  buttonMask = 0;
  if (Fl::event_button1())
    buttonMask |= 1 << 0;
  if (Fl::event_button2())
    buttonMask |= 1 << 1;
  if (Fl::event_button3())
    buttonMask |= 1 << 2;
#if defined(FL_BUTTON4) && defined(FL_BUTTON5)
  if (Fl::event_button4())
    buttonMask |= 1 << 7;
  if (Fl::event_button5())
    buttonMask |= 1 << 8;
#endif

  return buttonMask;
}

Win2VNCOverlay::Win2VNCOverlay(Viewport* viewport_)
  // Note: the two argument constructor is important, as it makes sure
  //       we don't accidentally end up as a child of some other widget
  : Fl_Window(1, 1), viewport(viewport_), controlling(false),
    edge(Win2VNCRight), screenX(0), screenY(0), screenW(1), screenH(1),
    workX(0), workY(0), workW(1), workH(1),
    remoteW(1), remoteH(1), remoteX(0), remoteY(0),
    lastX(0), lastY(0), cursorHidden(false), startTime(0)
{
  border(0);
  // We position ourselves manually, covering an entire screen
  force_position(1);
  end();
}

Win2VNCOverlay::~Win2VNCOverlay()
{
  restoreLocalCursor();
}

void Win2VNCOverlay::takeControl(int entryX, int entryY)
{
  int screen;
  int entryLocalX, entryLocalY;
  struct timeval now;

  if (controlling)
    return;

  viewport->win2vncGetRemoteSize(&remoteW, &remoteH);
  if ((remoteW <= 0) || (remoteH <= 0)) {
    vlog.error("Unknown remote screen size, cannot forward input");
    return;
  }

  edge = win2vncEdgeParam();

  screen = Fl::screen_num(entryX, entryY);
  Fl::screen_xywh(screenX, screenY, screenW, screenH, screen);
  Fl::screen_work_area(workX, workY, workW, workH, screen);
  if ((screenW < 2 * RECENTER_MARGIN) || (screenH < 2 * RECENTER_MARGIN)) {
    vlog.error("Local screen is too small for input forwarding");
    return;
  }

  // Put the pointer on the opposite side of the screen, so the user has
  // the entire width (or height) of the local screen available before
  // we have to warp the pointer back to the middle. The remote pointer
  // enters from the corresponding edge of the remote screen.
  switch (edge) {
  case Win2VNCLeft:
    entryLocalX = screenX + screenW - 1;
    entryLocalY = entryY;
    remoteX = remoteW - 1;
    remoteY = ((entryY - screenY) * remoteH) / screenH;
    break;
  case Win2VNCTop:
    entryLocalX = entryX;
    entryLocalY = screenY + screenH - 1;
    remoteX = ((entryX - screenX) * remoteW) / screenW;
    remoteY = remoteH - 1;
    break;
  case Win2VNCBottom:
    entryLocalX = entryX;
    entryLocalY = screenY;
    remoteX = ((entryX - screenX) * remoteW) / screenW;
    remoteY = 0;
    break;
  case Win2VNCRight:
  default:
    entryLocalX = screenX;
    entryLocalY = entryY;
    remoteX = 0;
    remoteY = ((entryY - screenY) * remoteH) / screenH;
    break;
  }

  clampRemotePos();

  controlling = true;

  gettimeofday(&now, nullptr);
  startTime = now.tv_sec + now.tv_usec / 1000000.0;

  // The window has to cover the entire screen, or we will stop getting
  // pointer events as soon as the pointer moves off of it
  resize(screenX, screenY, screenW, screenH);
  show();

#if defined(WIN32)
  win32_make_window_transparent(fl_xid(this));
#elif defined(__APPLE__)
  cocoa_make_window_transparent(this);
#else
  x11_set_window_opacity(this, 0.02);
  // A pointer grab makes sure we get all motion events, even if
  // something else pops up on top of us
  x11_grab_pointer(fl_xid(this));
#endif

  hideLocalCursor();

  warpTo(entryLocalX, entryLocalY);

  // Let the remote system know where the pointer showed up
  sendPointer(0);

  vlog.debug("Forwarding local input to the remote session "
             "(entered at %d,%d)", remoteX, remoteY);
}

void Win2VNCOverlay::releaseControl()
{
  int returnX, returnY;

  if (!controlling)
    return;

  controlling = false;

  // Don't leave any buttons pressed on the remote system
  sendPointer(0);

#if !defined(WIN32) && !defined(__APPLE__)
  x11_ungrab_pointer(fl_xid(this));
#endif

  restoreLocalCursor();

  hide();

  // Put the local pointer back where the user expects it, i.e. just
  // outside of the trigger strip, at the height (or width) where the
  // remote pointer left the remote screen
  switch (edge) {
  case Win2VNCLeft:
    returnX = workX + win2vncWidth + RETURN_GAP;
    returnY = workY + (remoteY * workH) / remoteH;
    break;
  case Win2VNCTop:
    returnX = workX + (remoteX * workW) / remoteW;
    returnY = workY + win2vncWidth + RETURN_GAP;
    break;
  case Win2VNCBottom:
    returnX = workX + (remoteX * workW) / remoteW;
    returnY = workY + workH - 1 - win2vncWidth - RETURN_GAP;
    break;
  case Win2VNCRight:
  default:
    returnX = workX + workW - 1 - win2vncWidth - RETURN_GAP;
    returnY = workY + (remoteY * workH) / remoteH;
    break;
  }

  returnX = std::max(workX, std::min(workX + workW - 1, returnX));
  returnY = std::max(workY, std::min(workY + workH - 1, returnY));

  win2vncWarpPointer(returnX, returnY);

  vlog.debug("Local input returned to the local system");

  viewport->win2vncReleased();
}

int Win2VNCOverlay::handle(int event)
{
  switch (event) {
  case FL_ENTER:
  case FL_LEAVE:
    return 1;

  case FL_FOCUS:
    return 1;

  case FL_UNFOCUS:
    // Someone else (e.g. another application) took over, so we'd better
    // give the pointer back or the user will be stuck with an invisible
    // window eating all mouse events
    if (controlling) {
      struct timeval now;
      double elapsed;

      gettimeofday(&now, nullptr);
      elapsed = (now.tv_sec + now.tv_usec / 1000000.0) - startTime;
      if (elapsed > FOCUS_GRACE_MS / 1000.0) {
        vlog.debug("Lost focus whilst forwarding input");
        releaseControl();
      }
    }
    return 1;

  case FL_KEYDOWN:
  case FL_KEYUP:
  case FL_SHORTCUT:
    // Keyboard events are picked up by Viewport's system event handler,
    // we just have to make sure FLTK doesn't do anything else with them
    // (e.g. closing this window on Escape)
    return 1;

  case FL_PUSH:
  case FL_RELEASE:
  case FL_DRAG:
  case FL_MOVE:
  case FL_MOUSEWHEEL:
    if (controlling)
      handleMouse(event);
    return 1;

  case FL_HIDE:
    if (controlling)
      releaseControl();
    break;
  }

  return Fl_Window::handle(event);
}

void Win2VNCOverlay::draw()
{
#if defined(__APPLE__)
  // The window is fully transparent, so drawing nothing at all is
  // exactly what we want
#else
  // Elsewhere we may not get real transparency, but the window manager
  // will at least make this as unobtrusive as it can
  fl_color(FL_BLACK);
  fl_rectf(0, 0, w(), h());
#endif
}

void Win2VNCOverlay::handleMouse(int event)
{
  int rootX, rootY;
  int dx, dy;
  uint16_t buttonMask;

  // Note that we ask the system where the pointer is rather than using
  // the coordinates in the event. Events can be generated before we
  // warp the pointer, but be handled after, and we would then compute a
  // huge bogus movement from them. Asking the system always gives us
  // the current, true, position so the accumulated movement stays
  // correct no matter how the events are queued up.
  Fl::get_mouse(rootX, rootY);

  dx = rootX - lastX;
  dy = rootY - lastY;

  lastX = rootX;
  lastY = rootY;

  // Nothing happened, e.g. the event generated by our own warping
  if ((dx == 0) && (dy == 0) &&
      ((event == FL_MOVE) || (event == FL_DRAG)))
    return;

  remoteX += dx;
  remoteY += dy;

  clampRemotePos();

  if (escaped()) {
    releaseControl();
    return;
  }

  buttonMask = currentButtonMask();

  if (event == FL_MOUSEWHEEL) {
    uint16_t wheelMask;

    wheelMask = 0;
    if (Fl::event_dy() < 0)
      wheelMask |= 1 << 3;
    if (Fl::event_dy() > 0)
      wheelMask |= 1 << 4;
    if (Fl::event_dx() < 0)
      wheelMask |= 1 << 5;
    if (Fl::event_dx() > 0)
      wheelMask |= 1 << 6;

    // A quick press of the wheel "button", followed by an immediate
    // release below
    sendPointer(buttonMask | wheelMask);
  }

  sendPointer(buttonMask);

  recenterPointer();
}

void Win2VNCOverlay::sendPointer(uint16_t buttonMask)
{
  core::Point pos;

  pos.x = std::max(0, std::min(remoteW - 1, remoteX));
  pos.y = std::max(0, std::min(remoteH - 1, remoteY));

  viewport->win2vncSendPointer(pos, buttonMask);
}

bool Win2VNCOverlay::escaped() const
{
  switch (edge) {
  case Win2VNCLeft:
    return remoteX > (remoteW - 1 + ESCAPE_MARGIN);
  case Win2VNCTop:
    return remoteY > (remoteH - 1 + ESCAPE_MARGIN);
  case Win2VNCBottom:
    return remoteY < -ESCAPE_MARGIN;
  case Win2VNCRight:
  default:
    return remoteX < -ESCAPE_MARGIN;
  }
}

void Win2VNCOverlay::clampRemotePos()
{
  int minX, maxX, minY, maxY;

  // The pointer is allowed to go a bit outside of the remote screen,
  // but only on the side where the local system is
  minX = (edge == Win2VNCRight) ? -(ESCAPE_MARGIN + 1) : 0;
  maxX = (edge == Win2VNCLeft) ? (remoteW - 1 + ESCAPE_MARGIN + 1) :
                                 (remoteW - 1);
  minY = (edge == Win2VNCBottom) ? -(ESCAPE_MARGIN + 1) : 0;
  maxY = (edge == Win2VNCTop) ? (remoteH - 1 + ESCAPE_MARGIN + 1) :
                                (remoteH - 1);

  remoteX = std::max(minX, std::min(maxX, remoteX));
  remoteY = std::max(minY, std::min(maxY, remoteY));
}

void Win2VNCOverlay::warpTo(int x, int y)
{
  x = std::max(screenX, std::min(screenX + screenW - 1, x));
  y = std::max(screenY, std::min(screenY + screenH - 1, y));

  lastX = x;
  lastY = y;

  win2vncWarpPointer(x, y);
}

void Win2VNCOverlay::recenterPointer()
{
  if ((lastX >= (screenX + RECENTER_MARGIN)) &&
      (lastX < (screenX + screenW - RECENTER_MARGIN)) &&
      (lastY >= (screenY + RECENTER_MARGIN)) &&
      (lastY < (screenY + screenH - RECENTER_MARGIN)))
    return;

  // We're about to run out of local screen, so start over in the middle
  warpTo(screenX + screenW / 2, screenY + screenH / 2);
}

void Win2VNCOverlay::hideLocalCursor()
{
  if (cursorHidden)
    return;

  cursorHidden = true;

  cursor(FL_CURSOR_NONE);

#if defined(__APPLE__)
  cocoa_hide_cursor();
#endif
}

void Win2VNCOverlay::restoreLocalCursor()
{
  if (!cursorHidden)
    return;

  cursorHidden = false;

#if defined(__APPLE__)
  cocoa_show_cursor();
#endif

  cursor(FL_CURSOR_DEFAULT);
}
