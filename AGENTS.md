# mOS contributor instructions

Read [ARCHITECTURE.md](ARCHITECTURE.md), [docs/roadmap.md](docs/roadmap.md) and [docs/mos-app-catalog.md](docs/mos-app-catalog.md) first.

- The human maintainer decides. Keep changes focused; prefer a branch + PR; never merge without approval. Other contributors (and other assistants) work in this repo: check branches and open PRs before starting, and do not overwrite their work.
- **RF safety (shared with Mission-Link):** keep transmission disabled until the exact board, pinout, RF switch, antenna and Canadian (ISED) rules are verified and the maintainer approves. Never send plaintext private messages over RF before an encryption design exists.
- Do not claim range, encryption, delivery guarantees, or hardware compatibility without evidence. Never show a message as delivered because it was handed to a radio; only an accepted ACK means delivered. Report host tests, ESP-IDF builds and real-hardware tests separately.
- Components are portable C11 and must build with `-std=c11 -Wall -Wextra -Werror -pedantic`. Add a host test to `tests/run_host_tests.sh` for new logic and run it (also with `SANITIZE=1`) before pushing.
- Keep shell and app code on the `mos_canvas_t` / gesture abstractions; apps must not touch hardware directly. Display and touch hardware are not chosen yet.
- `prototypes/platformio-cpp/` is reference code, not the build. Do not extend it; port from it.
- Never commit secrets or private device identifiers.
