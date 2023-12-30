# Lighting Control System

This project describes how to build an LCM for your home. The code in here is unlikely to be generic but rather should be taken as an example.

## Build

If `index.html` changed - run `python minimise.py` to compile its `index.ccp` version.

Open the sketch and upload it.


## Development

The board is on fixed IP address. This is because:

1. Using DHCP proved to be slow and unreliable for the shield I have at hands.
2. It consumes 20% of memory.

When working on the static page, use `proxy.py` - it proxies all `/api` requests to the board and serves the static page locally. This simplifies the code on board (as it doesn't have to worry about CORS) and allows for quick iterations.
