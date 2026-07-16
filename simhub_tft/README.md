# SimHub setup

The Pico firmware listens on `COM12` at `115200` baud and accepts two
newline-terminated ASCII packet types:

```text
F;<rpm>;<speed-kmh>;<delta-seconds>
S;<gear>;<last-lap>;<max-rpm>
```

In SimHub, enable **Custom Serial Devices**, select `COM12`, use `115200 8N1`,
enable DTR, and add these two update messages.

## Fast data - 60 Hz

```ncalc
'F;' +
format(isnull([DataCorePlugin.GameData.NewData.Rpms], 0), '0') + ';' +
format(isnull([DataCorePlugin.GameData.NewData.SpeedKmh], 0), '0') + ';' +
format(isnull([PersistantTrackerPlugin.SessionBestLiveDeltaSeconds], 0), '0.000') +
'\n'
```

## State data - Changes only

```ncalc
'S;' +
isnull([DataCorePlugin.GameData.NewData.Gear], 'N') + ';' +
format(
  isnull([DataCorePlugin.GameData.NewData.LastLapTime], secondstotimespan(0)),
  'm\\:ss\\.fff'
) + ';' +
format(isnull([DataCorePlugin.GameData.NewData.MaxRpm], 0), '0') +
'\n'
```

SimHub property availability can vary by game. If a property is not recognized,
use **Insert property** and choose that game's equivalent for live delta, last
lap time, or maximum RPM; the packet format itself stays unchanged.

The parser accepts the fast packet at 60 Hz without blocking. Display writes
are coalesced to roughly 30 FPS because the TFT is intentionally running at the
known-stable 8 MHz SPI speed.
