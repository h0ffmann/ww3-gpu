# `WW_KOKKOS_LIBM` ON or OFF in a CUDA build of the Kokkos port

2026-10-18. A decision record for the owner, asked in the owner's session (goal `eb10363f`).
It compares the two values of one CMake option and recommends one value for each kind of build.
The decision is the owner's. Nothing here ran on a GPU (`AGENTS.md`, "Running on the GPU").

Tags in this page:

| Tag | Meaning |
|---|---|
| (v) | Checked: read or run on 2026-10-18, at the commit or file named |
| ⚠ | Assumption: not checked |
| **Position** | The author's recommendation, not a fact |
| **WIP** | Work that is designed or started, and not complete |

Related: [WFIP-0006](WFIPs/WFIP-0006-device-libm-bits.md),
[ADR-0014](ADRs.md#adr-0014),
[GPU_LIBM_BITS_202610.md](GPU_LIBM_BITS_202610.md),
[`gen_tables.cpp`](../kokkos/tools/libm_bits/gen_tables.cpp),
[`CMakeLists.txt`](../kokkos/src/ww_kokkos/CMakeLists.txt), [AGENTS.md](../AGENTS.md), and the
terms in [GLOSSARY.md](GLOSSARY.md).

Sources are read at `origin/main` `5b48e08`, unless another commit is named. The CUDA image is
read at `3d00e2c`, the head of branch `goal/fe2173f3-4827-ghcr-images` (PR #192).

## 1. The question

A CUDA build of the port has the option `WW_KOKKOS_LIBM`. Its default is ON (v,
`kokkos/src/ww_kokkos/CMakeLists.txt:310-311`). The CUDA Docker image of PR #192 sets it to OFF,
because the build fails with ON on Ubuntu (v, `docker/Dockerfile.cuda:55-58` at `3d00e2c`, and
the message of commit `3d00e2c`). The owner chose libdevice for the image.

The question: which value must each kind of CUDA build use, and what must change before the
image can use ON?

## 2. What ON and OFF do

**ON.** Each libm call of the kernels that `WW_KOKKOS_ALL` runs goes to device copies of the
functions of glibc 2.44 (`ww::libm`), through `kokkos/src/ww_kokkos/libm_device.hpp` (v,
`CMakeLists.txt:305-309`; `docs/GLOSSARY.md:708`). W3OUTG (the `resident` preset) uses them too
(v, `docs/WFIPs/WFIP-0006-device-libm-bits.md:160-164`). At build time, CMake compiles the host
program `gen_tables` with the host g++ and runs it (v, `CMakeLists.txt:318-333`). `gen_tables`
does these steps:

1. It finds the `libmvec.so.1` that the toolchain links (v, `gen_tables.cpp:160-167`).
2. It reads the hidden symbol `__svml_spow_data` from the `.symtab` or `.dynsym` of that library
   (v, `gen_tables.cpp:54-92`). The symbol is `LOCAL`, so only `.symtab` has it (v, `readelf -s`
   of the Nix glibc 2.44-25 `libmvec.so.1`: `773: ... 30848 OBJECT LOCAL ... __svml_spow_data`).
   If it does not find the symbol, it exits with code 3 (v, `gen_tables.cpp:168-169`).
3. It compares the library's constants with the literals that `glibc_mathf.hpp` writes, for
   `__svml_spow_data` and `__svml_s_trig_data`. A difference stops the build (v,
   `gen_tables.cpp:177-188`).
4. It decides if this host gets the fma or the sse2 build of glibc (`WW_LIBM_HOST_FMA`) (v,
   `gen_tables.cpp:170-171`).
5. It records what the `rcpps` instruction of this build host returns for each of the 2^23
   mantissas (v, `gen_tables.cpp:204-237`).

The tables become compile-time constants in the device code (v,
`kokkos/tools/libm_bits/glibc_mathf.hpp:611-622`). Thus the binary contains facts about the build
host: its glibc, its `rcpps` and its fma choice.

**OFF.** The kernels call NVIDIA's libdevice, as before #178. No `gen_tables` runs. A GPU run is
judged by the tolerances of `nccmp-tol` (v, `CMakeLists.txt:308-309`;
`docs/ADRs.md:1666-1679`).

**The CPU build.** The option does not exist there. The host branches call glibc as before (v,
`CMakeLists.txt:309`, `:310` `if(Kokkos_ENABLE_CUDA)`).

## 3. Comparison

| Property | ON (device copies of glibc 2.44) | OFF (libdevice) |
|---|---|---|
| Bits on the CPU (the host copies against glibc 2.44) | Equal on every input checked: all 2^32 floats for the unary float functions, 3.6 × 10^10 to 7.9 × 10^10 pairs for `powf`, `atan2f`, libmvec `powf` (v, `docs/GPU_LIBM_BITS_202610.md:22-25`, Intel Core i9-14900) | Not applicable: the CPU build does not use libdevice |
| Bits on the GPU against the Fortran | Expected equal: the SASS has the same fused and unfused operations as glibc's x86 fma build (v, `GPU_LIBM_BITS_202610.md:163-176`). Not measured ⚠ | Not equal in general. libdevice uses other algorithms (v, `AGENTS.md:319-321`); how far the output moves is not measured ⚠ (`ADR-0014:51-52`) |
| Verified on a device? | No. `scripts/libm_bits.sh --device` needs the owner at the terminal (v, `GPU_LIBM_BITS_202610.md:283`; `WFIP-0006:181`) | No. The first supervised run reads the `nccmp-tol` table (v, `ADR-0014:62`) |
| Coverage | The kernels of `WW_KOKKOS_ALL` and W3OUTG. The kernels of other switches (ST1, ST2, ST3, ST6, FLX1 to FLX5, IC4) still call libdevice (v, `WFIP-0006:165-166`) | All kernels call libdevice |
| Portability across CPUs | Tied to the build host: its `rcpps`, its fma choice, its glibc. On the i9-14900, the rounded `rcpps` differs from the rounded exact reciprocal for 661 269 of 8 388 608 mantissas (v, `GPU_LIBM_BITS_202610.md:272-274`). Another vendor's `rcpps` can give other bits ⚠ (`gen_tables.cpp:207-209`). The run host must be the build host, or have the same glibc build and `rcpps`. Nothing checks this at run time (v, `WFIP-0006:152-153`) | One binary for all hosts. The result does not depend on the CPU, and it is not the Fortran's bits on any CPU |
| Build requirements | A `libmvec.so.1` with `.symtab`; a host g++; glibc 2.44's constants; the build CPU is the run CPU | The CUDA toolchain only |
| Speed on the device | Not measured ⚠. The copies compute in double precision. An RTX 4090 runs FP64 at about 1/64 of its FP32 rate, an H100 at about 1/2 ⚠ (vendor datasheets, `WFIP-0006:193-197`). W3OUTG's kernel went from 96 to 161 registers and from 112 B to 576 B of stack (v, `kokkos/src/fortran_iface/PATCH.md:1727`, sm_89, ptxas) | Not measured ⚠. libdevice's float functions are expected to be faster ⚠ |
| What ADR-0014 and WFIP-0006 say | WFIP-0006 makes ON the default "as the bit-for-bit rule asks"; the owner decides after the H100 cost is measured (v, `WFIP-0006:228-229`). Until the device boxes of §7 pass, ADR-0014's tolerance verdict applies to ON too (v, `AGENTS.md:322-330`) | ADR-0014: the GPU run is judged by tolerance, never with a wider tolerance (v, `ADR-0014:31-38`). WFIP-0006 keeps OFF as the way out (v, `WFIP-0006:196-197`) |

Thus, at this commit, a GPU run is judged by tolerance with ON and with OFF. ON gives a chance
of bit-for-bit results. OFF gives a portable binary.

## 4. What is incomplete with ON

- **No device run.** The device copies were not compared with glibc on a GPU (v,
  `GPU_LIBM_BITS_202610.md:283`). Denormals, conversions and table reads on the device are the
  part that the host checks cannot show ⚠ (`WFIP-0006:207-209`).
- **Other switches.** The kernels of ST1, ST2, ST3, ST6, FLX1 to FLX5 and IC4 call libdevice (v,
  `WFIP-0006:165-166`). A run with these switches is not bit for bit with ON.
- **Unit 5 is not done.** No script check makes sure that no libdevice function stays on the device
  path (v, `WFIP-0006:158-159`, `:180`). **WIP.**
- **The build host is the reference.** The `rcpps` table and the fma choice come from the build
  host (v, `gen_tables.cpp:1-14`). A binary built on one CPU and run on another CPU, for example
  from an image or on a CI runner, can disagree with the Fortran on the run host ⚠. No run-time
  check finds this (v, `WFIP-0006:152-153`).
- **The glibc version is fixed.** The copies are glibc 2.44's. A Fortran that links another glibc
  has another reference. Example: glibc 2.44 made the double `tanh` correctly rounded with
  CORE-MATH (v, `GPU_LIBM_BITS_202610.md:69`), so a glibc 2.43 `tanh` (W3SDB1) can give other
  bits ⚠. `gen_tables` checks the libmvec constants only, not the scalar functions (v,
  `gen_tables.cpp:177-188`).
- **NaN payloads** can differ between x86 and CUDA; `test.comp` compares bytes ⚠
  (`WFIP-0006:198-200`).

## 5. Options to make ON work in the Docker image

These are evaluated here, not implemented.

### A. Read the symbol from Ubuntu's debug file

The stripped library has a `.gnu_debuglink` and a build ID. The package `libc6-dbg` installs the
matching file `/usr/lib/debug/.build-id/<xx>/<rest>.debug`. Checks on this host (Ubuntu 26.04.1,
`libc6-dbg` 2.43-2ubuntu2.4), all (v):

| Check | Result |
|---|---|
| `readelf -S` of `/usr/lib/x86_64-linux-gnu/libmvec.so.1` | no `.symtab`; `.gnu_debuglink` present |
| `readelf -s` of the `.debug` file of build ID `6efea554...` | `__svml_spow_data` at vaddr `0x39780`, 30848 B; `__svml_s_trig_data` at `0x41000`, 1792 B; section 18 `.rodata` is `NOBITS` |
| `readelf -lW` of the stripped library | the `R` `PT_LOAD` maps vaddr `0x29000` at file offset `0x29000` |
| 30848 B at offset `0x39780` and 1792 B at `0x41000`, Ubuntu 2.43 against Nix 2.44-25 | byte-identical, both tables |

The method works. `gen_tables` must take the symbol from the debug file and its bytes from the
stripped library, through the `PT_LOAD` that holds the vaddr. The debug file has no bytes there
(`NOBITS`). The image must install `libc6-dbg` at the same version as `libc6` ⚠. That the CUDA
base image has glibc 2.43 is an assumption ⚠.

What A does not solve: the Fortran in the image links Ubuntu's glibc, not 2.44. The libmvec
tables are equal, but the scalar functions can differ (section 4) ⚠. The `rcpps` table is still
the build host's.

### B. Build the image with the pinned Nix toolchain

The Nix glibc 2.44-25 keeps `.symtab` with the symbol (v, `readelf -S` and `readelf -s`, above).
The Fortran and the copies then use the same glibc, as on the lab machine. This is the only option
that removes the glibc version difference. Costs: a larger image and a build through Nix in Docker
(the `#cuda` shell of `picasso/labs/pratico`) ⚠ (size not measured). It does not solve the
`rcpps` table of the build host.

### C. Make the tables on the run host at first start

This solves the `rcpps` table and the fma choice: the run host makes its own. But the tables are
compile-time constants in the device code (v, `glibc_mathf.hpp:611-622`). Thus C needs one of
two things: a compile of the port on the run host, or a change that loads the tables at run time.
The second is a design change to WFIP-0006, units 2 and 4. A smaller step is a guard: at start,
compare a fingerprint of the run host's `rcpps` and fma choice with the values of the build, and
stop if they differ. The guard does not make the bits, but it prevents a false claim ⚠ (not
designed).

## 6. Recommendation

**Position.**

1. Keep ON as the default of the library. Use ON on the lab machine, where the pinned Nix
   toolchain gives glibc 2.44 and the build host is the run host. This is the only configuration
   that can become bit for bit on the GPU.
2. Keep OFF in the distributed CUDA image, as the owner chose. With ON, the image has a glibc that
   is not 2.44 and the `rcpps` table of the CI build host. A run on another CPU can then look
   bit for bit while it is not. OFF does not claim more than ADR-0014 gives.
3. Record each GPU timing with ON and with OFF, as `WFIP-0006:196-197` asks. Then the speed cost
   of ON is a number.

This position changes on these conditions:

- If `scripts/libm_bits.sh --device` or the first supervised `just smoke-ww3-gpu gpu` shows a
  difference that the copies cannot remove, ON gives no benefit. Then OFF can be the default.
- If the measured H100 cost of ON is large, the owner can make OFF the default for performance
  work and keep ON for parity runs (`WFIP-0006:228-229`).
- If the image moves to the Nix toolchain (option B) and gets a run-time guard for `rcpps` and
  the fma choice (option C, the smaller step), ON in the image is safe. Option A alone is not
  sufficient.
- If WFIP-0006's device boxes pass and an ADR replaces ADR-0014 for the routed parts, ON is
  necessary for every result that claims bit-for-bit parity.

## 7. How to repeat

Run these commands from the repository root. They do not use a GPU.

1. Read the option and the build step:

   ```bash
   sed -n 305,336p kokkos/src/ww_kokkos/CMakeLists.txt
   sed -n 160,243p kokkos/tools/libm_bits/gen_tables.cpp
   ```

2. Read the image setting:

   ```bash
   git show 3d00e2c:docker/Dockerfile.cuda | sed -n 55,58p
   ```

3. Find the symbol in the Nix and the Ubuntu libraries:

   ```bash
   N=/nix/store/h4wfwic161kxrr74jlzla5lsm28hgary-glibc-2.44-25/lib/libmvec.so.1
   U=/usr/lib/x86_64-linux-gnu/libmvec.so.1
   readelf -s "$N" | grep -E '__svml_(spow|s_trig)_data'
   readelf -S "$U" | grep -E 'symtab|debuglink'
   readelf -n "$U" | grep 'Build ID'
   readelf -s /usr/lib/debug/.build-id/6e/fea554b9c0984e37b425b1b47eb1bfe08b01f7.debug \
     | grep -E '__svml_(spow|s_trig)_data'
   readelf -lW "$U" | grep LOAD
   ```

4. Compare the tables of the two libraries:

   ```bash
   python3 -I -c "
   def rd(p, o, n):
       with open(p, 'rb') as f:
           f.seek(o); return f.read(n)
   N = '$N'; U = '$U'
   print(rd(U, 0x39780, 30848) == rd(N, 0x39780, 30848), rd(U, 0x41000, 1792) == rd(N, 0x41000, 1792))"
   ```

   The result on 2026-10-18 was `True True`.

5. Run the host checks of the copies: `scripts/libm_bits.sh all 4` (`GPU_LIBM_BITS_202610.md`).
6. Do not run the device check without the owner. The owner runs
   `scripts/libm_bits.sh --device`, which asks for a typed "yes".
