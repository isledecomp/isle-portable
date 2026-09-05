# LEGO Island guided installer

A web-based re-creation of the original 1997 LEGO Island setup wizard that walks
players through installing `isle-portable` on every supported platform. It is a
single static page (`index.html`) plus the assets in `assets/`, and is deployed
to GitHub Pages from `master` by `.github/workflows/pages.yml` (the workflow
enables Pages on first run; the site is served from the repository's Pages URL
with all asset paths relative, so it works under any sub-path).

## Assets

* `background.png`, `welcome.png`, `select.png`, `ready.png`, `complete.png` are
  the bitmaps embedded in the original installer (`INSTALL.EXE`, a Wise
  Installation System package on the LEGO Island 1.0 CD-ROM). They were
  inflated from the Wise overlay and converted losslessly to indexed PNG.
* `98.css` and the `ms_sans_serif*` web fonts come from
  [98.css](https://github.com/jdan/98.css) (MIT, see `LICENSE.98.css`) and
  provide the Windows 9x widget styling and the pixel-accurate MS Sans Serif.
* `favicon.png` is the game's own icon (`ISLE/res/isle.ico`).

## Working on it

Open the folder with any static file server, for example:

```sh
python3 -m http.server -d installer 8000
```

Useful URL parameters while editing:

* `#windows`, `#switch`, ... preselects a platform (also written back as you go,
  so the link can be shared).
* `?page=welcome|select|ready|install|complete&step=N` jumps to a page.
* `?help=1` opens the help box, `?exit=1` the exit confirmation.
* `?check=1` renders every page of every platform and appends a report
  (`<pre id="check">`) of any text that would overflow its box or leave a
  lone short word on its last line. Run it in a fresh (incognito) headless
  browser so a cached copy of the page is not measured.

All platform instructions live in the `PLATFORMS` array in `index.html`.
Download links point at the `continuous` release, so they never go stale.
