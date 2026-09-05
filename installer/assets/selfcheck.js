/* Test hooks for the guided installer. Loaded by index.html only when the page is opened with
   ?check=1 (layout self-check), ?keytest=1 (keyboard mnemonics) or ?dragtest=1 (window dragging).
   Results are appended to the document as <pre> elements with the matching id. */
(function () {
  'use strict';
  var T = window.__isleTest, $ = T.$, state = T.state, render = T.render, PLATFORMS = T.PLATFORMS;

  function report(id, text) {
    var pre = document.createElement('pre'); pre.id = id; pre.textContent = text;
    document.body.appendChild(pre);
  }

  if (T.query.keytest === '1') {
    var log = [];
    var key = function (k, alt, el) { (el || document).dispatchEvent(new KeyboardEvent('keydown', { key: k, altKey: !!alt, bubbles: true, cancelable: true })); };
    key('n'); log.push('n->' + state.page);
    key('b'); log.push('b->' + state.page);
    key('N', true); log.push('altN->' + state.page);
    key('Escape'); log.push('esc->exitbox:' + !T.exitbox.hidden);
    key('n'); log.push('no->exitbox:' + !T.exitbox.hidden + ',page:' + state.page);
    key('F1'); log.push('f1->help:' + !T.helpbox.hidden);
    key('c'); log.push('c->help:' + !T.helpbox.hidden);
    state.page = 'select'; state.platform = PLATFORMS[0]; render(); $('listbox').focus(); key('n', false, $('listbox'));
    log.push('list n->' + state.platform.id + ',page:' + state.page);
    report('keytest', log.join(' | '));
  }

  if (T.query.dragtest === '1') {
    var bar = T.wizard.querySelector('.title-bar'), before = T.wizard.offsetLeft + ',' + T.wizard.offsetTop;
    var pe = function (type, x, y) { bar.dispatchEvent(new PointerEvent(type, { bubbles: true, clientX: x, clientY: y, pointerId: 1, button: 0 })); };
    pe('pointerdown', 300, 300); pe('pointermove', 340, 360); pe('pointerup', 340, 360);
    report('dragtest', 'scale ' + T.scale() + ' before ' + before + ' after ' + T.wizard.offsetLeft + ',' + T.wizard.offsetTop);
  }

  // ?check=1 renders every page for every platform, in both layouts, and reports text that
  // would overflow its box, lone short words on a last line, and clipping boxes whose text
  // sits closer than 2 stage pixels to a clipped edge (a fractional zoom can then cut it).
  if (T.query.check === '1') {
    var run = function () {
      var lines = [], checkScale = T.stage.getBoundingClientRect().width / T.stage.offsetWidth;

      var orphans = function (container) {
        var out = [], els = container.querySelectorAll('p, li');
        for (var i = 0; i < els.length; i++) {
          var walker = document.createTreeWalker(els[i], NodeFilter.SHOW_TEXT), node, words = [];
          while ((node = walker.nextNode())) {
            var text = node.nodeValue, re = /\S+/g, m;
            while ((m = re.exec(text))) {
              var r = document.createRange(); r.setStart(node, m.index); r.setEnd(node, m.index + m[0].length);
              var rc = r.getBoundingClientRect();
              if (rc.width) words.push({ top: Math.round(rc.top / checkScale / 13), w: rc.width / checkScale, t: m[0] });
            }
          }
          if (words.length < 2) continue;
          var lastTop = words[words.length - 1].top;
          if (lastTop === words[0].top) continue;
          var lastWords = words.filter(function (w) { return w.top === lastTop; });
          var width = lastWords.reduce(function (a, w) { return a + w.w; }, 0);
          if ((lastWords.length === 1 && lastWords[0].t.length <= 20) || width < 40) {
            var byLine = {};
            words.forEach(function (w) { byLine[w.top] = (byLine[w.top] || []).concat(w.t); });
            out.push('ORPHAN "' + lastWords.map(function (w) { return w.t; }).join(' ') + '" LINES=' + JSON.stringify(byLine));
          }
        }
        return out.join(' ');
      };

      var clipRisk = function () {
        var out = [], els = T.stage.querySelectorAll('*');
        for (var i = 0; i < els.length; i++) {
          var el = els[i];
          if (el.hidden || !el.firstChild || el.closest('[hidden]')) continue;
          var cs = getComputedStyle(el);
          if (cs.overflowX === 'visible' && cs.overflowY === 'visible') continue;
          var box = el.getBoundingClientRect(), padLeft = box.left + el.clientLeft, padTop = box.top + el.clientTop;
          var walker = document.createTreeWalker(el, NodeFilter.SHOW_TEXT), node;
          while ((node = walker.nextNode())) {
            if (!node.nodeValue.trim()) continue;
            var r = document.createRange(); r.selectNodeContents(node);
            var rc = r.getBoundingClientRect();
            if (!rc.width) continue;
            if (rc.left - padLeft < 2 * checkScale - 0.01 || rc.top - padTop < 1 * checkScale - 0.01) {
              out.push(' CLIPRISK(' + (el.id || el.className) + ':' + node.nodeValue.trim().slice(0, 12) + ')'); break;
            }
          }
        }
        return out.join('');
      };

      var measure = function (prefix) {
        for (var pi = 0; pi < PLATFORMS.length; pi++) {
          state.platform = PLATFORMS[pi];
          state.page = 'ready'; render();
          var b = $('body');
          lines.push(prefix + state.platform.id + ' ready ' + b.scrollHeight + '/' + b.clientHeight + (b.scrollHeight > b.clientHeight ? ' OVERFLOW' : '') + (b.scrollWidth > b.clientWidth + 2 ? ' WIDE' : '') + ' ' + orphans(b) + clipRisk());
          for (var si = 0; si < state.platform.steps.length; si++) {
            state.page = 'install'; state.step = si; render();
            var sb = $('step-body'), hp = T.installing.querySelector('.hdr');
            lines.push(prefix + state.platform.id + ' step' + si + ' ' + sb.scrollHeight + '/' + sb.clientHeight + (sb.scrollHeight > sb.clientHeight ? ' OVERFLOW' : '') + (sb.scrollWidth > sb.clientWidth + 2 ? ' WIDE' : '') +
                       (hp.scrollWidth > hp.clientWidth + 2 ? ' PATH-CLIPPED' : '') + ' ' + orphans(sb) + clipRisk());
          }
        }
        state.page = 'welcome'; render(); lines.push(prefix + 'welcome ' + orphans($('body')) + clipRisk());
        state.page = 'select'; render(); lines.push(prefix + 'select ' + clipRisk());
        state.page = 'complete'; render(); lines.push(prefix + 'complete ' + orphans($('body')) + clipRisk());
      };

      var saved = { page: state.page, platform: state.platform, step: state.step };
      var canvas = document.createElement('canvas').getContext('2d');
      canvas.font = '11px "Pixelated MS Sans Serif"';
      lines.push('welcome-line2-width ' + canvas.measureText('Press the Next button to start the installation. You can press the').width);
      if (document.fonts) { var faces = []; document.fonts.forEach(function (f) { faces.push(f.family + '/' + f.weight + ':' + f.status); }); lines.push('fonts ' + faces.join(' ')); }
      lines.push('help ' + orphans($('help-msg')));
      var wasPortrait = T.stage.classList.contains('portrait');
      T.stage.classList.remove('portrait'); measure('');
      T.stage.classList.add('portrait'); measure('portrait ');
      T.stage.classList.toggle('portrait', wasPortrait);
      state.page = saved.page; state.platform = saved.platform; state.step = saved.step; render();
      report('check', lines.join('\n'));
    };
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(run); else run();
  }
})();
