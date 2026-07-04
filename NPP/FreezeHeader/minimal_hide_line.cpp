#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include "src/Scintilla.h"
#include "src/Notepad_plus_msgs.h"
#include "src/PluginInterface.h"

NppData nppData;

HWND getCurrentSci() {
	int which = 0;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
	return which == 0 ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

static const int HEADER_LINES = 5;
static bool freezeEnabled = false;
static int hiddenAfterHeader = 0;

static WNDPROC oldSciProc = NULL;
static HWND currentSciHwnd = NULL;

static int lastThumbTrack = 0;
static bool thumbDragging = false;
static int thumbDragStartTrack = 0;

void updateFreezeView(HWND hwnd, int newHidden) {
  if (!hwnd) return;
  int lc = (int)SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
  if (lc < 0) lc = 0;

  // Clamp hidden so we don't hide past end
  int maxH = (lc > HEADER_LINES) ? (lc - HEADER_LINES) : 0;
  if (newHidden < 0) newHidden = 0;
  if (newHidden > maxH) newHidden = maxH;

  // Always force the desired first body line (after header + hidden) to be visible.
  // This is key for slider UP: no matter what the previous hidden was,
  // we explicitly make lines starting at HEADER + newHidden visible.
  // Then we hide only the prefix we want hidden right after the header.
  int firstBody = HEADER_LINES + newHidden;
  if (firstBody < lc) {
    SendMessage(hwnd, SCI_SHOWLINES, firstBody, lc - 1);
  }

  // Keep the top 5 always visible
  SendMessage(hwnd, SCI_SHOWLINES, 0, HEADER_LINES - 1);

  // Hide exactly the current "scrolled past" prefix after the header
  if (newHidden > 0) {
    int endHide = HEADER_LINES + newHidden - 1;
    if (endHide >= HEADER_LINES) {
      SendMessage(hwnd, SCI_HIDELINES, HEADER_LINES, endHide);
    }
  }

  // Force the view to have the header at the top
  SendMessage(hwnd, SCI_SETFIRSTVISIBLELINE, 0, 0);

  // Lie to the scrollbar: full doc range, pos = logical scroll position
  SCROLLINFO si = {sizeof(si)};
  si.fMask = SIF_RANGE | SIF_POS;
  si.nMin = 0;
  si.nMax = (lc > 0) ? lc : 0;
  si.nPos = HEADER_LINES + newHidden;
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

  // Force a repaint so visibility + scrollbar changes are reflected immediately
  InvalidateRect(hwnd, NULL, FALSE);
  UpdateWindow(hwnd);

  hiddenAfterHeader = newHidden;
}

LRESULT CALLBACK SciSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (freezeEnabled) {
    if (msg == WM_MOUSEWHEEL) {
      int delta = GET_WHEEL_DELTA_WPARAM(wParam);
      int n = 1;
      if (delta < 0) {
        // wheel down -> hide one more line
        updateFreezeView(hwnd, hiddenAfterHeader + n);
      } else {
        // wheel up -> unhide one (the helper will show excess lines and clamp)
        updateFreezeView(hwnd, hiddenAfterHeader - n);
      }
      return 0; // consume
    }

    if (msg == WM_VSCROLL) {
      int code = LOWORD(wParam);

      if (code == SB_LINEDOWN || code == SB_PAGEDOWN) {
        int n = (code == SB_PAGEDOWN) ? 3 : 1;
        updateFreezeView(hwnd, hiddenAfterHeader + n);
        return 0;
      }
      if (code == SB_LINEUP || code == SB_PAGEUP) {
        // Full scrollbar up functionality: unhide gradually (one or a few lines)
        int n = (code == SB_PAGEUP) ? 3 : 1;
        updateFreezeView(hwnd, hiddenAfterHeader - n);
        return 0;
      }
      if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION) {
        int lc = (int)SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
        if (lc < 0) lc = 0;

        // Pre-force our desired full-document range so the thumb reports usable positions
        SCROLLINFO force = {sizeof(force)};
        force.fMask = SIF_RANGE;
        force.nMin = 0;
        force.nMax = (lc > 0) ? lc : 0;
        SetScrollInfo(hwnd, SB_VERT, &force, FALSE);

        SCROLLINFO si = {sizeof(si)};
        si.fMask = SIF_TRACKPOS | SIF_POS;
        GetScrollInfo(hwnd, SB_VERT, &si);
        int track = (code == SB_THUMBPOSITION) ? si.nPos : si.nTrackPos;
        if (track < 0) track = 0;

        // Full scrollbar functionality:
        // Treat the thumb position as the logical scroll offset into the document.
        // The frozen header occupies the first HEADER_LINES "units".
        // So desired hidden = track - HEADER_LINES
        int desired = track - HEADER_LINES;
        updateFreezeView(hwnd, desired);

        // Force the scrollbar thumb to stay at the exact position the user dragged it to
        SCROLLINFO thumbPos = {sizeof(thumbPos)};
        thumbPos.fMask = SIF_POS;
        thumbPos.nPos = track;
        SetScrollInfo(hwnd, SB_VERT, &thumbPos, TRUE);

        lastThumbTrack = track;
        thumbDragging = false;
        thumbDragStartTrack = 0;
        return 0;
      }
    }
  }
  return CallWindowProc(oldSciProc, hwnd, msg, wParam, lParam);
}

void setupScrollIntercept(HWND sci) {
  if (sci == currentSciHwnd) return;
  if (oldSciProc && currentSciHwnd) {
    SetWindowLongPtr(currentSciHwnd, GWLP_WNDPROC, (LONG_PTR)oldSciProc);
  }
  currentSciHwnd = sci;
  oldSciProc = (WNDPROC)SetWindowLongPtr(sci, GWLP_WNDPROC, (LONG_PTR)SciSubclassProc);
  hiddenAfterHeader = 0;
  lastThumbTrack = 0;
  thumbDragging = false;
  thumbDragStartTrack = 0;
}

void applyFreeze() {
  HWND sci = getCurrentSci();
  if (!sci) return;

  setupScrollIntercept(sci);

  if (freezeEnabled) {
    updateFreezeView(sci, hiddenAfterHeader);
  } else {
    int lc = (int)SendMessage(sci, SCI_GETLINECOUNT, 0, 0);
    SendMessage(sci, SCI_SHOWLINES, 0, lc);
    hiddenAfterHeader = 0;
    if (oldSciProc && currentSciHwnd) {
      SetWindowLongPtr(currentSciHwnd, GWLP_WNDPROC, (LONG_PTR)oldSciProc);
      oldSciProc = NULL;
      currentSciHwnd = NULL;
    }
  }
}

void toggleFreeze() {
  freezeEnabled = !freezeEnabled;

  HWND sci = getCurrentSci();
  if (sci) {
    if (!freezeEnabled) {
      int lc = (int)SendMessage(sci, SCI_GETLINECOUNT, 0, 0);
      SendMessage(sci, SCI_SHOWLINES, 0, lc);
      hiddenAfterHeader = 0;
      lastThumbTrack = 0;
      thumbDragging = false;
      thumbDragStartTrack = 0;
      if (oldSciProc && currentSciHwnd) {
        SetWindowLongPtr(currentSciHwnd, GWLP_WNDPROC, (LONG_PTR)oldSciProc);
        oldSciProc = NULL;
        currentSciHwnd = NULL;
      }
    } else {
      hiddenAfterHeader = 0;
      lastThumbTrack = 0;
      thumbDragging = false;
      thumbDragStartTrack = 0;
      applyFreeze();
    }
  }
}

extern "C" __declspec(dllexport) void setInfo(NppData data) {
	nppData = data;
	// Optional debug - remove or comment for production
	// MessageBoxW(NULL, L"Freeze Header plugin loaded", L"Debug", MB_OK);
}

extern "C" __declspec(dllexport) const wchar_t* getName() {
	return L"FreezeHeader";
}

static FuncItem funcItem[1];

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {
	*nbF = 1;
	memset(&funcItem[0], 0, sizeof(FuncItem));
	wcscpy(funcItem[0]._itemName, L"Toggle Freeze Top 5 Lines");
	funcItem[0]._pFunc = toggleFreeze;
	funcItem[0]._init2Check = false;
	funcItem[0]._pShKey = NULL;
	return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* n) {
	// Subclass handles the scroll interception now.
	if (n->nmhdr.code == NPPN_BUFFERACTIVATED) {
		// New buffer: reset scroll offset state for the new document
		hiddenAfterHeader = 0;
		lastThumbTrack = 0;
		thumbDragging = false;
		thumbDragStartTrack = 0;
		applyFreeze();
	}

	if (n->nmhdr.code == SCN_UPDATEUI && freezeEnabled) {
		// Only pin the view to header top + fake bar pos after v-scroll updates.
		// Do NOT call updateFreezeView here (it could re-apply hides). 
		// This lets the direct "unhide all" in the thumb up handler stick.
		if (n->updated & SC_UPDATE_V_SCROLL) {
			HWND sci = getCurrentSci();
			if (sci) {
				SendMessage(sci, SCI_SETFIRSTVISIBLELINE, 0, 0);
				int lc = (int)SendMessage(sci, SCI_GETLINECOUNT, 0, 0);
				SCROLLINFO si = {sizeof(si)};
				si.fMask = SIF_RANGE | SIF_POS;
				si.nMin = 0;
				si.nMax = (lc > 0) ? lc : 0;
				si.nPos = HEADER_LINES + hiddenAfterHeader;
				SetScrollInfo(sci, SB_VERT, &si, TRUE);
			}
		}
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT msg, WPARAM w, LPARAM l) {
	return TRUE;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
#endif

BOOL APIENTRY DllMain(HANDLE, DWORD, LPVOID) { return TRUE; }
