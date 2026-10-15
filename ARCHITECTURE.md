# mOS / Mission Link: Architecture

Shared project context. Paste this into any assistant (Claude, Gemini, ChatGPT) at the start of a session so everyone works from the same decisions. Update it whenever a decision changes.

Repo: https://github.com/Innygr/mOS

## Projects

**Mission Link**: a LoRa community communication system. Goal: carry communication from Mission, BC to Vancouver, BC (roughly 60 km).

**mOS**: a custom smartwatch-style operating system for the Seeed Xiao ESP32S3. It natively supports and runs Mission Link.

## Decided

- **Watch hardware:** Xiao ESP32S3 plus an SX1262 LoRa module (e.g. Seeed Wio-SX1262 kit). The Xiao ESP32S3 has no LoRa radio of its own.
- **Network topology:** stationary mesh nodes are the main connectors (the backbone). Watches do not need to reach each other directly over long distances.
- **Watch-to-network link:** each watch has its own LoRa radio and connects to the mesh through a stationary node (Option A). BLE/Wi-Fi is not the primary link.
- **OS role:** mOS runs Mission Link natively. The LoRa radio is a first-class system service, with the UI on top of it.
- **Protocol:** completely custom mesh protocol. It is not built on Meshtastic, MeshCore, or any existing mesh stack.

## Design implications (derived, not yet confirmed)

- Stationary nodes should sit on high ground with proper antennas; the long hops between Mission and Vancouver happen between nodes.
- Watch antenna placement in the case will strongly affect range.
- The radio is the main battery cost: plan scheduled receive windows, deep sleep, and adjustable spreading factor and transmit power.
- LoRa airtime is scarce, so messages and protocol overhead should be compact.
- Messages need store-and-forward for coverage gaps.

## Open questions

- Custom protocol design: packet format, routing method between nodes, addressing, acknowledgements and retries, time sync, and how nodes and watches share airtime.
- Messaging scope: text only, or also GPS positions, SOS/alerts, community channels?
- Node discovery, selection, and handoff as the wearer moves.
- Watch identity and addressing that stay constant across nodes.
- Security: authentication of watch to node, message encryption.
- Display and input hardware (round or square, touch, buttons).
- Framework/toolchain (Arduino, ESP-IDF, PlatformIO, etc.).
- Node hardware, power (solar/battery), and site locations along the route.
- Regulatory check: ISED rules for the 902-928 MHz band in Canada (power, channel use, duty cycle).
