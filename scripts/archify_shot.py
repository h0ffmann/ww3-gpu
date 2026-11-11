#!/usr/bin/env python3
"""archify_shot — a PNG of one local HTML page, taken by headless Chrome over its DevTools pipe.

    archify_shot.py <chrome> <page.html> <out.png> <width> <height>

scripts/archify.sh png calls it. Chrome's own --screenshot flag never returns in some containers
(a cloud agent session, 2026-11-11); the pipe is what Archify's browser check uses and does.
The picture is taken at twice the size, for a sharp README image, once the page's fonts are ready.
"""
import base64
import json
import os
import subprocess
import sys
import tempfile


def main(chrome, page, out, width, height):
    w, h = int(width), int(height)
    to_chrome_r, to_chrome_w = os.pipe()
    from_chrome_r, from_chrome_w = os.pipe()
    with tempfile.TemporaryDirectory() as profile:
        args = [chrome, "--headless=new", "--remote-debugging-pipe", "--disable-gpu",
                "--hide-scrollbars", "--no-first-run", "--no-default-browser-check",
                f"--user-data-dir={profile}", "about:blank"]
        if hasattr(os, "getuid") and os.getuid() == 0:
            args.insert(1, "--no-sandbox")

        def child():  # Chrome reads fd 3 and writes fd 4; dup first, so the two cannot collide
            r, w = os.dup(to_chrome_r), os.dup(from_chrome_w)
            os.dup2(r, 3)
            os.dup2(w, 4)

        proc = subprocess.Popen(args, pass_fds=(3, 4), preexec_fn=child,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.close(to_chrome_r)
        os.close(from_chrome_w)
        tx, rx = os.fdopen(to_chrome_w, "wb"), os.fdopen(from_chrome_r, "rb")
        buf, seq = b"", 0

        def send(method, params=None, session=None):
            nonlocal buf, seq
            seq += 1
            msg = {"id": seq, "method": method, "params": params or {}}
            if session:
                msg["sessionId"] = session
            tx.write(json.dumps(msg).encode() + b"\0")
            tx.flush()
            while True:
                while b"\0" not in buf:
                    chunk = rx.read1(1 << 16)
                    if not chunk:
                        raise SystemExit("archify_shot: Chrome closed the pipe")
                    buf += chunk
                raw, buf = buf.split(b"\0", 1)
                reply = json.loads(raw)
                if reply.get("id") == seq:
                    if "error" in reply:
                        raise SystemExit(f"archify_shot: {method}: {reply['error']}")
                    return reply["result"]

        try:
            target = send("Target.createTarget", {"url": "about:blank"})["targetId"]
            s = send("Target.attachToTarget", {"targetId": target, "flatten": True})["sessionId"]
            send("Emulation.setDeviceMetricsOverride",
                 {"width": w, "height": h, "deviceScaleFactor": 2, "mobile": False}, s)
            send("Page.enable", session=s)
            send("Page.navigate", {"url": "file://" + os.path.abspath(page)}, s)
            ready = ("new Promise(r => { const go = () => document.fonts.ready.then(() =>"
                     " setTimeout(() => r(true), 300));"
                     " document.readyState === 'complete' ? go() : addEventListener('load', go); })")
            send("Runtime.evaluate", {"expression": ready, "awaitPromise": True}, s)
            shot = send("Page.captureScreenshot", {"format": "png"}, s)
            with open(out, "wb") as f:
                f.write(base64.b64decode(shot["data"]))
        finally:
            proc.kill()
            proc.wait()


if __name__ == "__main__":
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    main(*sys.argv[1:])
