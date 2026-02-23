# DefaultCab — Phone testing & deployment

This project uses Web Bluetooth (BLE). A few important notes for testing on phones and deploying to HTTPS:

- Web Bluetooth support:
  - Chrome on Android supports Web Bluetooth (recent versions).
  - Safari on iOS and most iOS browsers do NOT support Web Bluetooth — iPhone cannot use BLE from a webpage today.

- Secure context requirement:
  - Web Bluetooth requires a secure context (HTTPS) or `localhost`. If you open the files via `file://` or HTTP, BLE will be blocked.

Quick ways to test from your phone:

1) Use a public HTTPS tunnel (ngrok):

   - Install and run ngrok pointing to your local HTTP server (example serves current folder on port 80):

```bash
# run a simple local static server (from project folder)
# using Python 3
python -m http.server 8000

# in another terminal, expose it via ngrok
ngrok http 8000
```

Open the https:// URL shown by ngrok in Chrome on your Android phone.

2) GitHub Pages:

   - Push this repo to GitHub and enable GitHub Pages for the repo (deploy from `main` or `gh-pages`). Your site will be served over HTTPS automatically.

3) Localhost on your development machine:

   - If you can access your machine as `localhost` from the phone (e.g., via USB tethering or network), running a local HTTPS server or using `localhost` will work.

Notes:
- If you see the app message "Bluetooth not available on this device/browser" — try Chrome on Android over HTTPS or use a Chromebook/desktop for BLE access.
- For a long-term alternative (if you need iPhone support), consider adding a WebSocket relay on the ESP32 that the phone can talk to over plain HTTPS — ESP32 can host an HTTP endpoint or WebSocket server and you can proxy commands via a secure server.

If you want, I can:
- Add a deploy script or `package.json` with a small `serve` task.
- Set up a simple HTTPS self-signed dev server for local testing.
- Add a WebSocket relay example for non-BLE phones.

Tell me which of those you'd like next.