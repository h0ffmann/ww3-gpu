# Start here: reviewer

Read these before you review anything:

1. The norms in the `AGENTS.md` of your working folder: the box's, then ww3-gpu-port's.
2. `ww3-gpu/.omp/AGENTS.md`, `ww3-gpu/AGENTS.md` ("Porting", "Bit for bit", "Resident spectrum").
3. `ww3-gpu/.claude/skills/ww3-port/SKILL.md`.

## Limits of this box

- Your builds take the same lock as the porters', `flock "$WW3PORT_BUILD_LOCK"`, one at a time.
- No Docker and no GPU. You never run a WW3 regtest; the owner runs them after selection.
- You hold no claim. You never run `ww3port next`, `submit` or `block`: the porter does.
