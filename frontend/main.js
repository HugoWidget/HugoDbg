const isSW = typeof require == 'function';
const apiKey = '7c5a1bae0eae82d8246c6cb70be0beb5';
const channel = 'channel-0a44efbc3';
const ipcRenderer = require('electron').ipcRenderer;

let vKeyboardInstance = null;

// 执行脚本通信
function doEval(script) {
    ipcRenderer.send(channel, { apiKey, action: 'exec', data: script });
}

ipcRenderer.on(channel+'-reply', (event, data) => {
    window.__ipc_ret = data;
})

// 兼容获取require
let r;
if (typeof require('electron').remote == 'undefined' && typeof require == 'function') {
    r = require;
} else if (typeof require('electron').remote != 'undefined') {
    r = require('electron').remote.require;
}

// 核心模块
const fs = r('fs');
const path = r('path');
const os = r('os');
const configFilePath = path.join(os.homedir(), '.hugodbg.config');

// 配置读写
function loadConfig() {
    try {
        return JSON.parse(fs.readFileSync(configFilePath, 'utf-8'));
    } catch (e) {
        saveConfig({});
        return {};
    }
}
function saveConfig(newConfig) {
    fs.writeFileSync(configFilePath, JSON.stringify(newConfig));
}

let globalConfig = loadConfig();



// 配置自动生效
if (globalConfig.plugins) doEval('');
if (globalConfig.backgroundImage) {
    let times = 0;
    const timer = setInterval(() => {
        document.querySelectorAll('[class^="screenLock__bg-filter"], [class^="screenLock__bg"]')
            .forEach(e => e.style.backgroundImage = `url("${globalConfig.backgroundImage}")`);
        if (++times > 1500) clearInterval(timer);
    }, 1);
}
if (globalConfig.autoHideInfo) {
    let count = 0;
    const timer = setInterval(() => {
        document.querySelectorAll('span').forEach(e => {
            if (e.textContent.includes('设备ID') || e.textContent.includes('学校代码')) e.remove();
        });
        document.querySelectorAll('[class^="screenLock__bg"]').forEach(e => e.style.backgroundImage = '');
        if (++count > 5000) clearInterval(timer);
    }, 1);
}

if (globalConfig.disableHelper) return;

// 版本信息
function showVersion() {
    if (typeof process === 'undefined' || !process.versions) {
        DivDialog.alert('环境不合法，无法获取版本信息', { title: '错误', width: 350 });
        return;
    }
    const v = process.versions;
    const other = Object.entries(v).filter(([k]) => !['electron','chrome','node','v8','modules'].includes(k.toLowerCase()));
    let html = `<pre style="margin:0;font-family:Consolas;font-size:14px;">`;
    html += `<b>核心组件:</b><br>  Electron: ${v.electron}<br>  ├─ Chromium: ${v.chrome}<br>  ├─ Node.js: ${v.node}<br>  └─ V8: ${v.v8}<br><br>`;
    html += `<b>其他模块 (ABI ${v.modules}):</b><br>`;
    other.forEach(([n, v], i) => html += `  ${i===other.length-1?'└─':'├─'} ${n}: ${v}<br>`);
    html += `</pre>`;
    new DivWindow({ title: '详细版本信息', content: html, width: 280, height: 420 });
}

// 解锁锁屏
function unlockScreen() {
    isSW ? require('electron').ipcRenderer.send('windowMessage', { eventName: 'stopScreenLock', data: !0 }) : DivDialog.alert('环境不合法', { width: 250 });
}

// 隐藏信息
function hideInfo() {
    let text = '';
    document.querySelectorAll('span').forEach(e => {
        if (e.textContent.includes('设备ID') || e.textContent.includes('学校代码')) {
            text += e.textContent + '\n';
            e.remove();
        }
    });
    document.querySelectorAll('[class^="screenLock__bg"]').forEach(e => e.style.backgroundImage = '');
    DivDialog.alert(text || '未找到设备ID/学校代码', { width: 150 });
}

// 虚拟键盘
function toggleVirtualKeyboard() {
    if (!vKeyboardInstance) {
        try {
            vKeyboardInstance = new VirtualKeyboard({ 
                inputSelector: 'input, textarea', 
                autoEnable: false 
            });
        } catch (e) {
            DivDialog.alert('失败了', { title: '错误' });
            console.error(e);
            return;
        }
    }

    if (vKeyboardInstance.state.isVisible) {
        vKeyboardInstance.close();
        document.removeEventListener('focusin', vKeyboardInstance._handleFocusIn);
    } else {
        // 打开时绑定焦点监听，自动追踪输入框
        document.addEventListener('focusin', vKeyboardInstance._handleFocusIn);
        // 如果当前已有聚焦的输入框，直接设置
        const activeEl = document.activeElement;
        if (activeEl && activeEl.matches('input, textarea')) {
            vKeyboardInstance.open(activeEl);
        } else {
            vKeyboardInstance.open();
        }
    }
}

// JS控制台
function openMiniConsole() {
    try { new MiniConsole() } catch (e) { DivDialog.alert('失败了', { title: '错误' }); console.error(e); }
}

// 图片链接验证
function isValidImageUrl(url) {
    if (typeof url !== 'string' || !url.trim()) return false;
    const protocol = url.startsWith('http://') || url.startsWith('https://') || url.startsWith('file:///');
    const ext = ['.jpg','.jpeg','.png','.gif','.bmp','.webp','.svg'].some(e => url.toLowerCase().endsWith(e));
    return protocol && ext;
}

// 锁屏背景修改
let bgToolLock = false;
function bgTool() {
    if (bgToolLock) return;
    bgToolLock = true;

    const css = `
<style>
.bg-tool{padding:20px;height:100%;display:flex;flex-direction:column;gap:15px;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto}
.bg-tool label{font-size:14px;color:#333}
#bg-url{width:100%;padding:4px;font-size:14px;border:1px solid #ccc;border-radius:4px}
.bg-tool-btns{display:flex;gap:10px;margin-top:auto}
.bg-tool-btns button{padding:4px 16px;font-size:14px;border:none;border-radius:2px;cursor:pointer;background: rgb(26,144,255);color:#fff}
#restore{margin-right:auto;background:#d9534f}
#restore:hover{background:#c9302c}
.bg-tool-btns button:hover{background:#005a9e}
.bg-tool-btns button:disabled{opacity:.5;cursor:not-allowed}
</style>`;

    const html = `
<div class="bg-tool">
<label>图片链接 (http/https/file):</label>
<input type="text" id="bg-url" placeholder="输入图片链接...">
<div class="bg-tool-btns">
<button id="restore">还原</button>
<button id="preview">预览</button>
<button id="save">保存</button>
</div></div>`;

    const dom = document.createElement('div');
    dom.innerHTML = css + html;

    new DivWindow({
        title: '锁屏背景修改', width: 380, height: 200, resizable: true,
        content: dom, onClose: () => bgToolLock = false
    });

    const ipt = document.getElementById('bg-url');
    const [pre, save, res] = [document.getElementById('preview'), document.getElementById('save'), document.getElementById('restore')];

    const restore = () => {
        globalConfig.backgroundImage = '';
        ipt.value = '';
        saveConfig(globalConfig);
        document.querySelectorAll('[class^="screenLock__bg"]').forEach(e => e.style.backgroundImage = '');
        updateBtn();
    };
    const updateBtn = () => {
        const val = ipt.value.trim();
        const valid = isValidImageUrl(val);
        pre.disabled = !valid;
        save.disabled = !valid || val === globalConfig.backgroundImage;
        res.disabled = !globalConfig.backgroundImage;
    };

    ipt.addEventListener('input', updateBtn);
    pre.addEventListener('click', () => {
        document.querySelectorAll('[class^="screenLock__bg"]').forEach(e => e.style.backgroundImage = `url("${ipt.value.trim()}")`);
    });
    save.addEventListener('click', () => {
        const val = ipt.value.trim();
        document.querySelectorAll('[class^="screenLock__bg"]').forEach(e => e.style.backgroundImage = `url("${val}")`);
        globalConfig.backgroundImage = val;
        saveConfig(globalConfig);
        updateBtn();
        save.textContent = '已保存!';
        setTimeout(() => save.textContent = '保存', 1500);
    });
    res.addEventListener('click', restore);
    ipt.value = globalConfig.backgroundImage;
    updateBtn();
}

// 控制指定按钮显示/隐藏
function updateSpecialButtons() {
    const isFull = isAlmostFullScreen();
    const unlockBtn = document.querySelector('.grid button[onclick="unlockScreen()"]');
    const hideInfoBtn = document.querySelector('.grid button[onclick="hideInfo()"]');
    
    if (unlockBtn) unlockBtn.style.display = isFull ? 'block' : 'none';
    if (hideInfoBtn) hideInfoBtn.style.display = isFull ? 'block' : 'none';
}

// 主界面样式与结构（已移除对 DivWindow/Dialog 的样式覆盖）
const htmlContent = `
<style>
:root{--bg:rgb(219,238,255);--btn:rgb(8,209,82);--btn-hover:rgb(6,180,68)}
*{box-sizing:border-box;margin:0;padding:0}
.main{width:100%;height:100%;display:flex;flex-direction:column;padding:15px;background:var(--bg);font-family:-apple-system}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(85px,1fr));gap:10px}
.grid button{padding:4px;font-size:13px;color:#fff;background:var(--btn);border:none;border-radius:2px;cursor:pointer;transition:.15s}
.grid button:hover{background:var(--btn-hover)}
#version{position:absolute;bottom:4px;right:12px;font-size:10px;color:#333;cursor:pointer}
#version:hover{color:#000}
</style>
<div class="main">
<div class="grid">
<button onclick="location.reload()">刷新页面</button>
<button onclick="unlockScreen()">解锁锁屏</button>
<button onclick="hideInfo()">隐藏信息</button>
<button onclick="bgTool()">锁屏背景修改</button>
<button onclick="toggleVirtualKeyboard()">虚拟键盘</button>
<button onclick="openMiniConsole()">JS控制台</button>
</div>
<div id="version" onclick="showVersion()">HugoDbg v1.0.0</div>
</div>
`;

// 全局挂载方法
window.unlockScreen = unlockScreen;
window.hideInfo = hideInfo;
window.bgTool = bgTool;
window.toggleVirtualKeyboard = toggleVirtualKeyboard;
window.openMiniConsole = openMiniConsole;
window.showVersion = showVersion;

let mainWindowInstance = null;
// 创建主窗口
function createMain() {
    if (mainWindowInstance) {
        mainWindowInstance.focus?.();
        return;
    }
    mainWindowInstance = new DivWindow({
        title: '主菜单', width: 320, height: 180, content: htmlContent,
        x: 100, y: 100, onClose: () => mainWindowInstance = null
    });
    setTimeout(updateSpecialButtons, 50);
}

// 全屏检测
function isAlmostFullScreen() {
    const w = window.screen.width;
    const h = window.screen.height;
    return window.innerWidth/w >= 0.95 && window.innerHeight/h >= 0.95;
}

// 悬浮按钮
const openBtn = document.createElement('div');
openBtn.id = 'open_btn';
Object.assign(openBtn.style, {
    position: 'fixed', top: '4px', left: '-4px', width: '50px', height: '50px',
    cursor: 'pointer', opacity: 0.5, zIndex: 9999, display: 'none'
});
document.body.appendChild(openBtn);
openBtn.addEventListener('click', createMain);

// 全屏按钮
const fullScreenBtn = document.createElement('div');
fullScreenBtn.innerHTML = '×';
Object.assign(fullScreenBtn.style, {
    position: 'fixed', top: '10px', right: '10px', width: '40px', height: '40px',
    lineHeight: '40px', textAlign: 'center', cursor: 'pointer', color: 'white',
    fontSize: '24px', fontWeight: 'bold', zIndex: 9999, display: 'none'
});
document.body.appendChild(fullScreenBtn);

// 双击/三击事件
let clickCount = 0;
let clickTimer = null;
fullScreenBtn.addEventListener('click', (e) => {
    e.preventDefault();
    clickCount++;
    clearTimeout(clickTimer);
    clickTimer = setTimeout(() => clickCount = 0, 300);

    if (clickCount === 2) {
        createMain();
    } else if (clickCount === 3) {
        unlockScreen();
        clickCount = 0;
    }
});

// 窗口大小检测
function checkWindowSize() {
    const full = isAlmostFullScreen();
    if (full) {
        // ========== 检测并处理 unlock.bin ==========
        try {
            const unlockFilePath = path.join(os.homedir(), 'unlock.bin');
            if (fs.existsSync(unlockFilePath)) {
                const content = fs.readFileSync(unlockFilePath, 'utf-8').trim();
                if (content === 'FullScreenOperation:Direct') {
                    fs.writeFileSync(unlockFilePath, 'FullScreenOperation:Assist', 'utf-8');
                    unlockScreen();
                } else if (content === 'FullScreenOperation:Disable') {
                    unlockScreen();
                }
            }
        } catch (e) {
            console.error('处理 unlock.bin 失败:', e);
        }
    }
    const large = window.innerWidth > 500 && window.innerHeight > 300;
    fullScreenBtn.style.display = full ? 'block' : 'none';
    openBtn.style.display = !full && large ? 'block' : 'none';
    updateSpecialButtons();
}

checkWindowSize();
window.addEventListener('resize', checkWindowSize);
window.addEventListener('fullscreenchange', checkWindowSize);