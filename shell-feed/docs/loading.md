# shell-feed: loading runbook

This is distilled from the research repo's capture history
(`ps4-shell-capture-research`, HANDOFF v50-v55 and `docs/goldhen_autorun.md`).
It describes how the probe this module is built from has actually been
loaded and run, person-confirmed on real hardware. shell-feed inherits this
runbook unchanged at Stage A, since Stage A changes packaging, not behaviour.

## GoldHEN AutoRun

GoldHEN v2.4b18.10 added **AutoRun**: GoldHEN settings -> Utilities ->
Payloader, with two sub-screens:

- **Launchpad** -- run a payload once, by hand, from the console (X).
- **Autorun** -- a queue of payloads that run automatically. Square in
  Launchpad adds an entry to this queue; each entry can be individually
  enabled/disabled, and Square removes an entry.

AutoRun fires when GoldHEN itself loads. Trial E6 (person-run, five boots)
confirmed: the `.bin`-style payload used in this project's loader is
accepted, it works on firmware 9.00, and payloads live under
`/data/payloads`.

## The L1 skip (safety net)

**Holding L1 while GoldHEN loads skips every AutoRun payload for that boot.**
This is the recovery path if an AutoRun'd payload is misbehaving: hold L1,
boot clean, go remove or disable the bad entry from Launchpad/Autorun, then
boot normally again. This was tested on a harmless build as part of Trial E6
and works as described.

This is a *manual* safety net. It assumes a person is holding a controller
at boot. If the jailbreak itself is automated (e.g. driven from a script
rather than a person pressing buttons each time), L1 doesn't help -- nobody
is there to hold it. That scenario doesn't describe this project's setup
today, which is why the formerly-planned boot-aware crash-loop guard (a
run-once marker replacement) was dropped from the plan rather than built:
L1 alone is sufficient recovery for how GoldHEN is actually loaded here.

## What's confirmed on hardware vs. what isn't

| Claim | Status |
|---|---|
| AutoRun accepts this payload format, works on 9.00 | **Person-confirmed**, Trial E6, ~5 boots |
| L1 skips AutoRun payloads | **Person-confirmed**, tested on a harmless build |
| Cold boot -> GoldHEN -> loader -> probe -> live send, no PC step | **Captured** (capture v23-E: 45.4 s boot, 1,801/1,801 sent in 60 s, 0 skipped, 0 crash-like lines) |
| Survives PS-button suspend/resume | **Captured** (capture v23-F: same probe instance runs through a ~13 s rest/wake, 0 reload, 0 skipped) and separately **person-confirmed** across ~6 rest/wake cycles with correct colours on resume |
| Survives rest mode (not just a quick suspend) | **Not captured in this repo's logs**; person-reported working across several cycles, but entry into rest mode and a *long* rest are still open |
| Which firmwares beyond 9.00 AutoRun covers | **Unverified** -- not tested here |
| v24 itself (report file, `[dev]` mirror, self-heal, gate table) on a console | **PC-checked only** as of the point this module was imported (see `docs/*/v24_A.md`, `v24_B.md` in the research repo) -- the Stage A hardware build/capture in this merge's own plan (step 8) is what first puts v24 specifically on a console under this repo's name |

## Quick runbook

1. Load GoldHEN normally (not holding L1).
2. If AutoRun is configured for this payload, it runs automatically on
   boot; otherwise, run it once from Launchpad.
3. Watch for `[SHELLPROBE24]` lines in klog on first boot to confirm it's
   running (self-heal and gate lines appear there). The second line it
   prints is `[SHELLPROBE24] BUILD: commit=... branch=... num=... built=...`:
   the git commit it was built from and the build time, so a capture shows
   exactly which build ran. The commit is HEAD when `make` ran, so commit your
   changes before building if you want the hash to mean something.
4. If a build misbehaves: reboot holding **L1** to skip it, then remove or
   disable the entry from GoldHEN's Payloader screen before trying again.
