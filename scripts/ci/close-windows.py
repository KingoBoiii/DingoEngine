#!/usr/bin/env python3
# Sends WM_DELETE_WINDOW to every top-level window that accepts it, so an app shuts down the
# way a user closing it would. Ubuntu 24.04's xdotool predates `windowquit`, and destroying the
# window instead skips the engine's teardown. Exits 1 when no window accepted it.
import sys

from Xlib import X, display, protocol

d = display.Display()
wm_protocols = d.intern_atom("WM_PROTOCOLS")
wm_delete = d.intern_atom("WM_DELETE_WINDOW")

sent = 0
for window in d.screen().root.query_tree().children:
    try:
        if wm_delete not in (window.get_wm_protocols() or []):
            continue
    except Exception:
        continue
    event = protocol.event.ClientMessage(window=window, client_type=wm_protocols, data=(32, [wm_delete, X.CurrentTime, 0, 0, 0]))
    window.send_event(event, event_mask=X.NoEventMask)
    sent += 1

d.flush()
sys.exit(0 if sent else 1)
