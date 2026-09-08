#!/usr/bin/env python3
"""Turn a media capture run into one scrollable HTML gallery.

Reads the layout MediaCaptureTest writes:
    <report folder>/chars/<name>/NNNNN.png + manifest.json
    <report folder>/stages/<path>/NNNNN.png + manifest.json
and writes <report folder>/report.html, which references the PNGs in place.

Assets carrying flags sort to the top of their grid. Flags are errors, census
anomalies and fight-progress checks; engine warnings are kept separately and
shown per asset without flagging it, because nearly every Mugen asset has some.
Only structural problems with the capture output itself are detected here.

Ticking assets in the report and saving the list writes debug/manualtest_list.txt,
which `fullcharactertest list` and `fullstagetest list` replay in the game.
"""

import argparse
import html
import json
import os
import sys

REASON_LABELS = {
    "interval": "interval",
    "round": "round change",
    "hit": "big hit",
    "entities": "entity spike",
    "anomaly": "anomaly",
    "stage_start": "start",
    "stage_idle": "idle",
    "stage_left": "left edge",
    "stage_right": "right edge",
    "stage_high": "high camera",
}


# ------------------------------------------------------------------ collecting


class Asset:
    def __init__(self, kind, name, folder):
        self.kind = kind
        self.name = name
        self.folder = folder
        self.seed = 0
        self.screen_size = (0, 0)
        self.captures = []
        self.flags = []
        self.warnings = []


def collect_assets(report_folder, kind, sub_folder):
    root = os.path.join(report_folder, sub_folder)
    if not os.path.isdir(root):
        return []

    assets = []
    for folder, _, files in os.walk(root):
        if not has_capture_output(files):
            continue
        name = os.path.relpath(folder, root).replace(os.sep, "/")
        assets.append(read_asset(report_folder, kind, name, folder, files))
    return assets


def has_capture_output(files):
    if "manifest.json" in files:
        return True
    return any(name.endswith(".png") for name in files)


def read_asset(report_folder, kind, name, folder, files):
    asset = Asset(kind, name, os.path.relpath(folder, report_folder).replace(os.sep, "/"))
    manifest_path = os.path.join(folder, "manifest.json")
    manifest = read_manifest(manifest_path)
    if manifest is None:
        asset.flags.append(describe_missing_manifest(manifest_path))
        asset.captures = [{"file": f, "reason": "unknown"} for f in sorted(files) if f.endswith(".png")]
        return asset

    asset.name = manifest.get("name", name)
    asset.seed = manifest.get("seed", 0)
    asset.screen_size = (manifest.get("screenWidth", 0), manifest.get("screenHeight", 0))
    asset.captures = manifest.get("captures", [])
    asset.flags, asset.warnings = split_flags_from_warnings(manifest)
    asset.flags += derive_asset_flags(asset)
    return asset


def describe_missing_manifest(path):
    if os.path.isfile(path):
        return "manifest unreadable (not valid JSON)"
    return "no manifest written (capture run interrupted?)"


def split_flags_from_warnings(manifest):
    warnings = list(manifest.get("warnings", []))
    flags = []
    for flag in manifest.get("flags", []):
        if flag.startswith("warning: "):
            warnings.append(flag)
        else:
            flags.append(flag)
    return flags, warnings


# Manifest text comes from Mugen assets, whose names and paths carry Shift-JIS and
# Latin-1 bytes. A capture run written before the escaping fix decodes with
# replacement rather than costing the whole asset its manifest.
def read_manifest(path):
    try:
        with open(path, "rb") as handle:
            return json.loads(handle.read().decode("utf-8", "replace"), strict=False)
    except (OSError, ValueError):
        return None


def derive_asset_flags(asset):
    if not asset.captures:
        return ["no screenshots captured"]
    return []


def get_life_series(captures, side):
    return [capture.get("life", [0, 0])[side] for capture in captures]


def get_maximum_round(captures):
    return max([capture.get("round", 0) for capture in captures] + [0])


def get_maximum_entities(captures):
    return max([capture.get("entities", 0) for capture in captures] + [0])


def sort_assets_with_flagged_first(assets):
    return sorted(assets, key=lambda asset: (0 if asset.flags else 1, asset.name.lower()))


# -------------------------------------------------------------------- rendering


def render_report(character_assets, stage_assets):
    parts = [REPORT_HEAD]
    parts.append(render_summary(character_assets, stage_assets))
    parts.append(render_section("Characters", "chars", character_assets))
    parts.append(render_section("Stages", "stages", stage_assets))
    parts.append(REPORT_TAIL)
    return "".join(parts)


def render_summary(character_assets, stage_assets):
    flagged = count_flagged(character_assets) + count_flagged(stage_assets)
    total = len(character_assets) + len(stage_assets)
    return (
        '<header>\n'
        '<h1>Release asset report</h1>\n'
        '<p class="summary">%d assets captured &middot; <strong>%d flagged</strong> '
        '(%d characters, %d stages)</p>\n'
        '<div class="controls">\n'
        '<input id="filter" type="search" placeholder="Filter by name…" autocomplete="off">\n'
        '<label><input id="flaggedonly" type="checkbox"> flagged only</label>\n'
        '<label><input id="pickedonly" type="checkbox"> picked only</label>\n'
        '<label><input id="fullsize" type="checkbox"> full-size shots</label>\n'
        '</div>\n'
        '</header>\n'
    ) % (total, flagged, len(character_assets), len(stage_assets))


def count_flagged(assets):
    return len([asset for asset in assets if asset.flags])


def render_section(title, anchor, assets):
    if not assets:
        return ""
    rows = "".join(render_asset_row(asset) for asset in sort_assets_with_flagged_first(assets))
    return '<section id="%s">\n<h2>%s <span class="count">%d</span></h2>\n%s</section>\n' % (
        anchor,
        html.escape(title),
        len(assets),
        rows,
    )


def render_asset_row(asset):
    classes = "asset flagged" if asset.flags else "asset"
    return '<article class="%s" data-name="%s">\n%s%s%s%s</article>\n' % (
        classes,
        html.escape(asset.name.lower(), quote=True),
        render_asset_heading(asset),
        render_asset_facts(asset),
        render_asset_warnings(asset),
        render_thumbnail_strip(asset),
    )


def render_asset_heading(asset):
    flags = "".join('<span class="flag">%s</span>' % html.escape(flag) for flag in asset.flags)
    return '<div class="assethead">%s<h3>%s</h3>%s</div>\n' % (
        render_asset_pick(asset),
        html.escape(asset.name),
        flags,
    )


def render_asset_pick(asset):
    return ('<label class="pick" title="tick for a manual recheck">'
            '<input type="checkbox" data-kind="%s" data-asset="%s"></label>') % (
        html.escape(asset.kind, quote=True),
        html.escape(asset.name, quote=True),
    )


def render_asset_warnings(asset):
    if not asset.warnings:
        return ""
    lines = "".join("<li>%s</li>" % html.escape(warning) for warning in asset.warnings)
    return '<details class="warnings"><summary>%d warnings</summary><ul>%s</ul></details>\n' % (
        len(asset.warnings),
        lines,
    )


def render_asset_facts(asset):
    facts = ["%d shots" % len(asset.captures)]
    if asset.captures:
        facts.append("rounds 1–%d" % get_maximum_round(asset.captures))
        facts.append("entities peak %d" % get_maximum_entities(asset.captures))
    facts.append("seed %d" % asset.seed)
    return '<div class="facts"><span>%s</span>%s</div>\n' % (
        "</span><span>".join(html.escape(fact) for fact in facts),
        render_life_sparkline(asset),
    )


def render_life_sparkline(asset):
    if asset.kind != "character" or len(asset.captures) < 2:
        return ""
    paths = "".join(
        render_life_path(get_life_series(asset.captures, side), side) for side in range(2)
    )
    return (
        '<svg class="spark" viewBox="0 0 100 20" preserveAspectRatio="none" '
        'role="img" aria-label="life over the captured fight">%s</svg>' % paths
    )


def render_life_path(series, side):
    maximum = max(series) or 1
    step = 100.0 / max(len(series) - 1, 1)
    points = " ".join(
        "%.2f,%.2f" % (index * step, 19.0 - (value / float(maximum)) * 18.0)
        for index, value in enumerate(series)
    )
    return '<polyline class="spark%d" points="%s" />' % (side, points)


def render_thumbnail_strip(asset):
    return '<div class="strip">%s</div>\n' % "".join(
        render_thumbnail(asset, capture) for capture in asset.captures
    )


def render_thumbnail(asset, capture):
    source = "%s/%s" % (asset.folder, capture.get("file", ""))
    caption = describe_capture(asset, capture)
    classes = "shot anomaly" if capture.get("note") else "shot"
    return '<button class="%s" data-full="%s" title="%s"><img loading="lazy" src="%s" alt="%s"><span>%s</span></button>' % (
        classes,
        html.escape(source, quote=True),
        html.escape(caption, quote=True),
        html.escape(source, quote=True),
        html.escape(caption, quote=True),
        html.escape(get_reason_label(capture)),
    )


def get_reason_label(capture):
    reason = capture.get("reason", "")
    return REASON_LABELS.get(reason, reason)


def describe_capture(asset, capture):
    parts = ["%s — %s" % (asset.name, REASON_LABELS.get(capture.get("reason", ""), "capture"))]
    if "frame" in capture:
        parts.append("frame %d (%.1fs)" % (capture["frame"], capture["frame"] / 60.0))
    if "round" in capture:
        parts.append("round %d" % capture["round"])
    if "life" in capture:
        parts.append("life %d / %d" % (capture["life"][0], capture["life"][1]))
    if "entities" in capture:
        parts.append("entities %d" % capture["entities"])
    if capture.get("note"):
        parts.append(capture["note"])
    return " · ".join(parts)


REPORT_HEAD = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Release asset report</title>
<style>
:root { color-scheme: dark; }
* { box-sizing: border-box; }
body { margin: 0; background: #14161a; color: #e6e8ec;
	font: 14px/1.5 ui-sans-serif, system-ui, "Segoe UI", sans-serif; }
header { position: sticky; top: 0; z-index: 5; padding: 16px 20px 12px;
	background: #14161aee; backdrop-filter: blur(6px); border-bottom: 1px solid #2a2e36; }
h1 { margin: 0 0 4px; font-size: 18px; }
.summary { margin: 0 0 10px; color: #98a0ae; }
.summary strong { color: #ffb454; }
.controls { display: flex; gap: 14px; align-items: center; }
.controls input[type=search] { flex: 0 1 320px; padding: 6px 10px; border-radius: 6px;
	border: 1px solid #333945; background: #1b1f26; color: inherit; }
.controls label { color: #98a0ae; display: flex; gap: 6px; align-items: center; }
section { padding: 8px 20px 28px; }
h2 { font-size: 15px; text-transform: uppercase; letter-spacing: .08em; color: #98a0ae;
	border-bottom: 1px solid #2a2e36; padding-bottom: 6px; }
h2 .count { color: #5c6473; font-weight: normal; }
.asset { padding: 12px 0; border-bottom: 1px solid #21252c; }
.asset.hidden { display: none; }
.asset.picked { background: #1a2330; box-shadow: inset 3px 0 0 #6aa9ff; padding-left: 9px; }
.assethead { display: flex; flex-wrap: wrap; gap: 8px; align-items: baseline; }
.pick { display: inline-flex; align-items: center; }
.pick input { width: 15px; height: 15px; accent-color: #6aa9ff; cursor: pointer; }
.warnings { margin: 0 0 8px; font-size: 12px; color: #6f7787; }
.warnings summary { cursor: pointer; color: #7d8695; width: fit-content; }
.warnings ul { margin: 6px 0 0; padding-left: 18px; max-height: 220px; overflow: auto; }
.warnings li { margin-bottom: 2px; word-break: break-word; }
.assethead h3 { margin: 0; font-size: 15px; font-weight: 600; }
.flagged .assethead h3 { color: #ffb454; }
.flag { background: #4a2018; color: #ff9a7a; border: 1px solid #6b2f22;
	border-radius: 999px; padding: 1px 9px; font-size: 12px; }
.facts { display: flex; flex-wrap: wrap; gap: 12px; align-items: center;
	color: #6f7787; font-size: 12px; margin: 2px 0 8px; }
.spark { width: 120px; height: 18px; }
.spark polyline { fill: none; stroke-width: 1.2; vector-effect: non-scaling-stroke; }
.spark0 { stroke: #6aa9ff; }
.spark1 { stroke: #ff7a90; }
.strip { display: flex; gap: 6px; overflow-x: auto; padding-bottom: 6px; }
.shot { flex: 0 0 auto; padding: 0; border: 1px solid #2a2e36; border-radius: 4px;
	background: #0e1013; cursor: zoom-in; overflow: hidden; position: relative; }
.shot:hover { border-color: #6aa9ff; }
.shot.anomaly { border-color: #ff7a55; }
.shot.anomaly span { background: #7a2a14e0; color: #ffd9cc; }
.shot img { display: block; height: 96px; width: auto; }
.fullsize .strip { flex-wrap: wrap; overflow-x: visible; }
.fullsize .shot { cursor: zoom-in; }
.fullsize .shot img { height: auto; width: 640px; max-width: calc(100vw - 48px); }
.shot span { position: absolute; left: 0; bottom: 0; right: 0; font-size: 10px;
	background: #000000b0; color: #cfd4dd; padding: 1px 4px; }
dialog { border: none; background: #0e1013; padding: 0; max-width: 96vw; max-height: 96vh; }
dialog::backdrop { background: #000000d0; }
dialog img { display: block; max-width: 96vw; max-height: 90vh; }
dialog p { margin: 0; padding: 8px 12px; color: #98a0ae; font-size: 12px; }
.lightboxbody { position: relative; }
.lightboxbody button { position: absolute; top: 0; bottom: 0; width: 90px; border: none;
	background: none; color: #ffffff; font-size: 40px; cursor: pointer; opacity: .35; }
.lightboxbody button:hover { opacity: 1; background: #00000060; }
.navprev { left: 0; }
.navnext { right: 0; }
#picks { position: fixed; left: 0; right: 0; bottom: 0; z-index: 6; display: none;
	gap: 12px; align-items: center; padding: 10px 20px; background: #191d24f2;
	border-top: 1px solid #333945; backdrop-filter: blur(6px); }
#picks.visible { display: flex; }
#picks strong { color: #6aa9ff; }
#picks button { padding: 6px 12px; border-radius: 6px; border: 1px solid #333945;
	background: #232833; color: inherit; cursor: pointer; font: inherit; }
#picks button:hover { border-color: #6aa9ff; }
#pickhint { color: #6f7787; font-size: 12px; }
body { padding-bottom: 56px; }
</style>
</head>
<body>
"""

REPORT_TAIL = """<dialog id="lightbox">
<div class="lightboxbody"><button class="navprev" title="previous (left arrow)">&lsaquo;</button><img alt=""><button class="navnext" title="next (right arrow)">&rsaquo;</button></div>
<p></p>
</dialog>
<div id="picks"><span><strong id="pickcount">0</strong> picked for a manual recheck</span>
<button id="pickcopy">Copy names</button><button id="picksave">Save manualtest_list.txt</button><button id="pickclear">Clear</button>
<span id="pickhint">save it into debug/, then run <code>fullcharactertest list</code></span></div>
<script>
(function () {
	var lightbox = document.getElementById('lightbox');
	var lightboxImage = lightbox.querySelector('img');
	var lightboxCaption = lightbox.querySelector('p');
	var shots = [];
	var shotIndex = 0;
	var assets = [];
	var assetIndex = 0;

	function getVisibleAssets() {
		return Array.prototype.slice.call(document.querySelectorAll('.asset:not(.hidden)'))
			.filter(function (asset) { return asset.querySelector('.shot'); });
	}

	function showAsset(index) {
		if (!assets.length) return;
		assetIndex = (index + assets.length) % assets.length;
		var asset = assets[assetIndex];
		shots = Array.prototype.slice.call(asset.querySelectorAll('.shot'));
		asset.scrollIntoView({block: 'center'});
		showShot(0);
	}

	function showShot(index) {
		if (!shots.length) return;
		shotIndex = (index + shots.length) % shots.length;
		var shot = shots[shotIndex];
		lightboxImage.src = shot.dataset.full;
		lightboxCaption.textContent = shot.title + '  \u00b7  shot ' + (shotIndex + 1) + '/' + shots.length
			+ (assets.length ? ('  \u00b7  asset ' + (assetIndex + 1) + '/' + assets.length + ' (\u2191\u2193)') : '');
	}

	function openShot(shot) {
		var asset = shot.closest('.asset');
		assets = getVisibleAssets();
		assetIndex = Math.max(assets.indexOf(asset), 0);
		shots = Array.prototype.slice.call((asset || shot.closest('.strip')).querySelectorAll('.shot'));
		showShot(shots.indexOf(shot));
		if (!lightbox.open) lightbox.showModal();
	}

	document.addEventListener('click', function (event) {
		if (event.target.closest('.navprev')) { showShot(shotIndex - 1); return; }
		if (event.target.closest('.navnext')) { showShot(shotIndex + 1); return; }
		var shot = event.target.closest('.shot');
		if (shot) { openShot(shot); return; }
		if (event.target === lightbox) lightbox.close();
	});

	document.addEventListener('keydown', function (event) {
		if (!lightbox.open) return;
		if (event.key === 'ArrowLeft') { showShot(shotIndex - 1); event.preventDefault(); }
		if (event.key === 'ArrowRight') { showShot(shotIndex + 1); event.preventDefault(); }
		if (event.key === 'ArrowUp') { showAsset(assetIndex - 1); event.preventDefault(); }
		if (event.key === 'ArrowDown') { showAsset(assetIndex + 1); event.preventDefault(); }
	});

	lightbox.addEventListener('close', function () { lightboxImage.src = ''; });

	var filter = document.getElementById('filter');
	var flaggedOnly = document.getElementById('flaggedonly');
	var pickedOnly = document.getElementById('pickedonly');
	function applyFilter() {
		var needle = filter.value.trim().toLowerCase();
		document.querySelectorAll('.asset').forEach(function (asset) {
			var matchesName = !needle || asset.dataset.name.indexOf(needle) !== -1;
			var matchesFlag = !flaggedOnly.checked || asset.classList.contains('flagged');
			var matchesPick = !pickedOnly.checked || asset.classList.contains('picked');
			asset.classList.toggle('hidden', !(matchesName && matchesFlag && matchesPick));
		});
	}
	filter.addEventListener('input', applyFilter);
	flaggedOnly.addEventListener('change', applyFilter);
	pickedOnly.addEventListener('change', applyFilter);

	var fullSize = document.getElementById('fullsize');
	var FULLSIZE_KEY = 'dolmexica-release-report-fullsize';
	function applyFullSize() {
		document.body.classList.toggle('fullsize', fullSize.checked);
		try { window.localStorage.setItem(FULLSIZE_KEY, fullSize.checked ? '1' : ''); } catch (e) {}
	}
	try { fullSize.checked = !!window.localStorage.getItem(FULLSIZE_KEY); } catch (e) {}
	fullSize.addEventListener('change', applyFullSize);
	applyFullSize();

	var STORAGE_KEY = 'dolmexica-release-report-picks';
	var bar = document.getElementById('picks');
	var count = document.getElementById('pickcount');
	var boxes = Array.prototype.slice.call(document.querySelectorAll('.pick input'));

	function getPickedBoxes() {
		return boxes.filter(function (box) { return box.checked; });
	}

	function getPickedLines() {
		return getPickedBoxes().map(function (box) { return box.dataset.kind + '\\t' + box.dataset.asset; });
	}

	function storePicks() {
		try { window.localStorage.setItem(STORAGE_KEY, JSON.stringify(getPickedLines())); } catch (e) {}
	}

	function restorePicks() {
		var stored = [];
		try { stored = JSON.parse(window.localStorage.getItem(STORAGE_KEY) || '[]'); } catch (e) { stored = []; }
		if (!stored.length) return;
		boxes.forEach(function (box) {
			if (stored.indexOf(box.dataset.kind + '\\t' + box.dataset.asset) !== -1) box.checked = true;
		});
	}

	function refreshPicks() {
		boxes.forEach(function (box) {
			box.closest('.asset').classList.toggle('picked', box.checked);
		});
		var picked = getPickedBoxes().length;
		count.textContent = picked;
		bar.classList.toggle('visible', picked > 0);
		applyFilter();
	}

	function copyText(text) {
		var area = document.createElement('textarea');
		area.value = text;
		document.body.appendChild(area);
		area.select();
		try { document.execCommand('copy'); } catch (e) {}
		document.body.removeChild(area);
	}

	function makeListFile() {
		return '# picked in report.html - replay with: fullcharactertest list / fullstagetest list\\n'
			+ getPickedLines().join('\\n') + '\\n';
	}

	boxes.forEach(function (box) {
		box.addEventListener('change', function () { refreshPicks(); storePicks(); });
	});

	document.getElementById('pickcopy').addEventListener('click', function () {
		copyText(getPickedBoxes().map(function (box) { return box.dataset.asset; }).join('\\n'));
	});

	document.getElementById('picksave').addEventListener('click', function () {
		var link = document.createElement('a');
		link.href = URL.createObjectURL(new Blob([makeListFile()], {type: 'text/plain'}));
		link.download = 'manualtest_list.txt';
		document.body.appendChild(link);
		link.click();
		document.body.removeChild(link);
		URL.revokeObjectURL(link.href);
	});

	document.getElementById('pickclear').addEventListener('click', function () {
		boxes.forEach(function (box) { box.checked = false; });
		refreshPicks();
		storePicks();
	});

	restorePicks();
	refreshPicks();
})();
</script>
</body>
</html>
"""


# ------------------------------------------------------------------------ main


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--report-folder", default="debug/report",
                        help="folder MediaCaptureTest wrote its captures to")
    parser.add_argument("--output", default=None,
                        help="where to write the HTML (default: <report folder>/report.html)")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    if not os.path.isdir(arguments.report_folder):
        sys.stderr.write("No capture folder at %s - run MediaCaptureTest first.\n" % arguments.report_folder)
        return 1

    character_assets = collect_assets(arguments.report_folder, "character", "chars")
    stage_assets = collect_assets(arguments.report_folder, "stage", "stages")
    if not character_assets and not stage_assets:
        sys.stderr.write("No captures found under %s.\n" % arguments.report_folder)
        return 1

    output = arguments.output or os.path.join(arguments.report_folder, "report.html")
    with open(output, "w", encoding="utf-8") as handle:
        handle.write(render_report(character_assets, stage_assets))

    print("%s: %d characters (%d flagged), %d stages (%d flagged)" % (
        output, len(character_assets), count_flagged(character_assets),
        len(stage_assets), count_flagged(stage_assets)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
