# shell-feed: support matrix

| | `plugin/` (sister) | `shell-feed/` |
|---|---|---|
| PS4 firmware | 9.00 and 12.50 | **9.00 only** |
| GoldHEN version | see main README | tested against GoldHEN v2.4b18.10 (AutoRun trials) |
| Rest mode | n/a (game-process plugin) | PS-button suspend/resume confirmed working (capture + person report); full rest-mode entry/long-rest not captured in logs, person-reported working across several cycles but treat as **not yet verified** |

shell-feed has only ever been built, loaded and captured on 9.00. It has not
been tried on 12.50 or any other firmware, and nothing here should be read
as implying it works there. If you try a different firmware, GoldHEN
version or PS4 strip configuration, open an issue with what you find --
same ask the main README already makes for the sister plugin.

This matrix will be updated once shell-feed has actually run on 12.50; until
then "9.00 only" is the honest claim, not a conservative placeholder.
