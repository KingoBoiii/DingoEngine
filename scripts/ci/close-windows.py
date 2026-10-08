#!/usr/bin/env python3
# Usage: close-windows.py [pid]
#
# Sends WM_DELETE_WINDOW to every top-level window that accepts it, so an app shuts down the
# way a user closing it would. Ubuntu 24.04's xdotool predates `windowquit`, and destroying the
# window instead skips the engine's teardown. With a pid, only that process's windows (by
# _NET_WM_PID, which GLFW sets) are closed, so it is safe on a desktop with other apps open.
# Under a window manager the root's children are its frames, so clients come from
# _NET_CLIENT_LIST when the window manager keeps one. Exits 1 when no window accepted it.
import sys

from Xlib import X, display, protocol

pid = int(sys.argv[1]) if len(sys.argv) > 1 else None

d = display.Display()
root = d.screen().root
wm_protocols = d.intern_atom("WM_PROTOCOLS")
wm_delete = d.intern_atom("WM_DELETE_WINDOW")
net_wm_pid = d.intern_atom("_NET_WM_PID")

windows = list(root.query_tree().children)
client_list = root.get_full_property(d.intern_atom("_NET_CLIENT_LIST"), X.AnyPropertyType)
if client_list:
    windows += [d.create_resource_object("window", w) for w in client_list.value]

sent = 0
for window in windows:
    try:
        if wm_delete not in (window.get_wm_protocols() or []):
            continue
        if pid is not None:
            owner = window.get_full_property(net_wm_pid, X.AnyPropertyType)
            if not owner or owner.value[0] != pid:
                continue
    except Exception:
        continue
    event = protocol.event.ClientMessage(window=window, client_type=wm_protocols, data=(32, [wm_delete, X.CurrentTime, 0, 0, 0]))
    window.send_event(event, event_mask=X.NoEventMask)
    sent += 1

d.flush()
sys.exit(0 if sent else 1)
