# The ww3-gpu-port rig, as files for piabrota/nix-os

The RigSpec, AgentSpecs and omp rules that [WFIP-0007](../../WFIP-0007-openrig-port-factory.md)
designs, written in the layout of piabrota/nix-os at `72de36b` (`openrig/specs/`, `omp/`) so the
nix-os pull request copies them as they are. They live here while the WFIP is a Draft; once nix-os
has them, nix-os is their only copy and this folder goes.

| file here | destination in piabrota/nix-os | what it is |
|---|---|---|
| `specs/rigs/ww3-gpu-port/rig.yaml` | `openrig/specs/rigs/ww3-gpu-port/rig.yaml` | the rig: pod `port`, members `porter-a`, `porter-b`, `reviewer`, all `runtime: omp` |
| `specs/agents/wg-shared/` | `openrig/specs/agents/wg-shared/` | the norms of every seat, merged into the seat's `AGENTS.md` |
| `specs/agents/wg-porter/` | `openrig/specs/agents/wg-porter/` | the porter: `agent.yaml`, its role and its start text |
| `specs/agents/wg-reviewer/` | `openrig/specs/agents/wg-reviewer/` | the reviewer, the same three files |
| `omp/ww3-gpu-port.yml` | `omp/ww3-gpu-port.yml` | the omp deny rules the seats add to `hermes/omp.yml` |

The AgentSpecs import `local:../shared`, the box norms that exist only in nix-os
(`openrig/specs/agents/shared/`), so `rig agent validate` passes only there.

Not validated ⚠: no `rig spec validate`, `rig agent validate` or `rig up --plan` has run on these
files. WFIP-0007's §7 lists the commands; they run on piabrota2 after the nix-os merge.
