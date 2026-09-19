# EchoCrypt

EchoCrypt is a narrative-driven, turn-based dungeon crawler rendered in ASCII.
The map is intentionally minimal and serves only as spatial guidance, while the
atmosphere is delivered through dynamic text descriptions, ambient sound, and an
AI Game Master that reacts to player actions.

## Features
- Minimal ASCII map (orientation only)
- Procedural narrative descriptions
- Ambient audio (echoes, dripping water, creaking doors)
- Multiplayer turn-based gameplay
- Authoritative server with Redis + PostgreSQL
- AI Game Master integrated into the server
- Designed for future multi-language support

## Project Structure
- `server/` — game logic, AI, world state, networking
- `client/` — ASCII rendering, narrative output, audio playback
- `db/` — PostgreSQL schema, seeds, migrations
- `docs/` — design documents and architecture notes

## Tech Stack
- C++ (server + client)
- Redis (runtime state, caching, pub/sub)
- PostgreSQL (persistent world data)
- SDL2 / WebAudio (sound)
- WebSockets / JSON (communication)

## Build & Run
Instructions will be added as the implementation progresses.

## License
MIT (or another license of your choice)

