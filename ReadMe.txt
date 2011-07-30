HttpMediaServer

A simple HTTP server for testing HTML5 media under rate-limited or
live-streaming conditions.

BUILDING: Compile on Linux using the build-linux.sh script, or on Windows
using with the Visual Studio project HttpMediaServer.sln.

USAGE: Just run the HttpMediaServer executable, and all files in the
working directory and (child folders) will be served.

On Windows the server runs on port 80, and port 8080 on Linux.

To serve files with rate limiting, append a query parameter rate=N to the
URL, where N is the rate in KB/s, e.g.:
http://localhost:80/video.webm?rate=200 serves video.webm at 200KB/s

To simulate a live stream, append a query parameter "live" to the URL, e.g.:
http://localhost:80/video.webm?live

Live streams are served without HTTP1.1 byte ranges being supported, and
without a Content-Length HTTP header.

These query parameters can of course be combined, e.g.:
http://localhost:80/video.webm?live&rate=200
