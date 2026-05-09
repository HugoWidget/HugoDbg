require('electron').BrowserWindow.getAllWindows()
    .forEach(w=>w.webContents.executeJavaScript(
        `(d=>{d=document.createElement('div');d.textContent='OK';d.style.cssText='position:fixed;top:10px;left:10px;color:green;font-weight:bold;z-index:9999;font-size:'+window.innerWidth*0.5+'px';document.body.appendChild(d);setTimeout(_=>d.remove(),3000)})()`
    ))