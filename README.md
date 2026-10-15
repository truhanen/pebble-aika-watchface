# Nyquist

An analog watchface for the Pebble Time 2 (emery). It uses bold Nyquist-style hands, 60 separate edge ticks (one per minute boundary, each 5 px long on the outer edge, which follows the screen frame's rounded corners (tunable `CF_CORNER_R`, 10 px by default), 5 px deep for 5-minute ticks and 1 px deep, set 1 px in from the screen edge, for 1-minute ticks, with sides pointing at the clock centre, all in the time text colour; each with a 2 px background-colour shadow), and configurable widgets along the left, right, top and bottom edges.

<p><img src="screenshots/readme/hero@2x.gif" width="200" alt="Nyquist watchface: a tour of the layouts and themes"></p>

<table>
  <tr>
    <td align="center"><img src="screenshots/readme/01-default.png" width="150" alt="Default layout"><br>Default layout</td>
    <td align="center"><img src="screenshots/readme/02-three-columns.png" width="150" alt="All three columns"><br>All three columns</td>
    <td align="center"><img src="screenshots/readme/03-mixed-fonts.png" width="150" alt="Mixed fonts"><br>Mixed fonts</td>
    <td align="center"><img src="screenshots/readme/04-unit-vs-icon.png" width="150" alt="Icon, unit and no identifier"><br>Icon, unit and no identifier</td>
    <td align="center"><img src="screenshots/readme/05-weather.png" width="150" alt="Rain, electricity price, forecast"><br>Rain, electricity price, forecast</td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/readme/06-dark-theme.png" width="150" alt="Dark theme"><br>Dark theme</td>
    <td align="center"><img src="screenshots/readme/07-light-theme.png" width="150" alt="Light theme"><br>Light theme</td>
    <td align="center"><img src="screenshots/readme/08-night.png" width="150" alt="Night mode"><br>Night mode</td>
    <td align="center"><img src="screenshots/readme/09-battery-warning.png" width="150" alt="Battery warning border"><br>Battery warning border</td>
  </tr>
</table>

Regenerate the images with `make screenshots` (`scripts/screenshots.sh`, which lists the scenes).

## Origins and credits

This watchface is a rework of [Sykerö TimeStyle](https://github.com/Sykero-Software/TimeStylePebble), which is itself a fork of [freakified/TimeStylePebble](https://github.com/freakified/TimeStylePebble) ("TimeStyle").

The widget system, the phone-side data sources (weather, electricity price, crypto, currency, Tuya) and the configuration page come from those projects. The analog clock face comes from the [Nyquist watchface](https://github.com/truhanen/pebble-nyquist-watchface).

## Development

```bash
npm install
pebble build            # also compiles src/ts -> src/pkjs
npm test                # JS unit tests + host-compiled C tests
```

Build with `TEST_HOOKS=1 pebble build` to enable the emulator test-hook AppMessages (see `src/c/test_hooks.c` and `scripts/emu.sh`).

## License

This is a combined work distributed under the **GNU General Public License v3.0 only** (GPL-3.0-only); see [`LICENSE`](LICENSE).

- The Nyquist clock face is GPL-3.0.
- The Sykerö TimeStyle additions are GPL-3.0-only, © 2026 Tuomas Airaksinen.
- The upstream TimeStyle code by Dan Tilden and the original TimeStyle contributors is under the **MIT License**. Its copyright and permission notice is kept in [`LICENSE.MIT`](LICENSE.MIT).
