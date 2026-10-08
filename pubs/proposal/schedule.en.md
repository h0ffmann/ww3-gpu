# Proposal schedule (EN)

The English copy of the schedule figure in [`mapas-mentais.pt.md`](mapas-mentais.pt.md#7-cronograma),
drawn from the table in [`en/08-schedule.md`](en/08-schedule.md). In the proposal's PDF and Word
files, that table is drawn as a month grid by [`../filters/cronograma.lua`](../filters/cronograma.lua),
in both languages.

```mermaid
%% figure: proposal-schedule
%% title: When does each activity of the project happen?
%% schedule: pubs/proposal/en/08-schedule.md
gantt
    title Undergraduate project schedule
    dateFormat YYYY-MM-DD
    axisFormat %m/%Y
    todayMarker off
    section Preparation
    Bibliographic review and configuration record        :a1, 2026-10-01, 2026-10-31
    Benchmark, simplified grids, comparator and profile   :a2, 2026-11-01, 2026-11-30
    section Optimisation without code changes
    Rung 1 · build options                                :b1, 2026-12-01, 2026-12-31
    Rung 2 · run configuration and per-routine tests      :b2, 2027-01-01, 2027-01-31
    section Rewriting
    Rung 3 · modern-Fortran refactoring                   :c1, 2027-02-01, 2027-02-28
    Rung 4 · C++/Kokkos kernels, agreement and H100       :c2, 2027-03-01, 2027-03-31
    section Closing
    Operational decision, final report                    :d1, 2027-04-01, 2027-04-30
    Defence                                               :milestone, d2, 2027-05-01, 0d
    section External milestone
    First WW4 release (ON 525, mid-2027)                  :milestone, w4, 2027-07-01, 0d
```

<details open>
<summary>How to read this figure</summary>

**Takeaway.** The project takes two academic periods, from October 2026 to April 2027, with one activity per month and the defence from May 2027.

**How to read.** Time runs left to right, in months; each bar is an activity, and each section groups related activities. The milestones (diamonds, no duration) are the defence and, as an outside reference, the expected first WW4 release.

**Not shown.** The date adjustments with the advisors that the text allows for.

**Evidence.** Table in `en/08-schedule.md` (v), against which `scripts/figures.py check` verifies the start month of each activity and the defence milestone; the WW4 milestone comes from `en/05-justification.md`, which places it in mid-2027 (v). ⚠ The days 2027-05-01 (the defence is "from" May) and 2027-07-01 only position the marks on the chart.

</details>
