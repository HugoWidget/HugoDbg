const isSW = typeof require == 'function';
const apiKey = '7c5a1bae0eae82d8246c6cb70be0beb5';
const channel = 'channel-0a44efbc3';
const ipcRenderer = require('electron').ipcRenderer;

let vKeyboardInstance = null;

// 执行脚本通信
function doEval(script) {
    ipcRenderer.send(channel, { apiKey, action: 'exec', data: script });
}

ipcRenderer.on(channel + '-reply', (event, data) => {
    window.__ipc_ret = data;
});

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
const configFilePath = path.join(os.homedir(), '.hugodbg.config');   // 用户配置
const hdConfigPath = path.join(os.homedir(), 'hdconfig.ini');        // 外部控制配置
let hdConfigRaw = null;
let hdConfigCache = null;   // 缓存解析后的 ini 配置

// ==================== 用户配置读写 ====================
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

// ==================== 功能函数 ====================
function showVersion() {
    if (typeof process === 'undefined' || !process.versions) {
        DivDialog.alert('环境不合法，无法获取版本信息', { title: '错误', width: 350 });
        return;
    }
    const v = process.versions;
    const other = Object.entries(v).filter(([k]) => !['electron', 'chrome', 'node', 'v8', 'modules'].includes(k.toLowerCase()));
    let html = `<pre style="margin:0;font-family:Consolas;font-size:14px;">`;
    html += `<b>核心组件:</b><br>  Electron: ${v.electron}<br>  ├─ Chromium: ${v.chrome}<br>  ├─ Node.js: ${v.node}<br>  └─ V8: ${v.v8}<br><br>`;
    html += `<b>其他模块 (ABI ${v.modules}):</b><br>`;
    other.forEach(([n, v], i) => html += `  ${i === other.length - 1 ? '└─' : '├─'} ${n}: ${v}<br>`);
    html += `</pre>`;
    new DivWindow({ title: '详细版本信息', content: html, width: 280, height: 420 });
}

function unlockScreen() {
    isSW ? require('electron').ipcRenderer.send('windowMessage', { eventName: 'stopScreenLock', data: !0 }) : DivDialog.alert('环境不合法', { width: 250 });
}

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
        document.addEventListener('focusin', vKeyboardInstance._handleFocusIn);
        const activeEl = document.activeElement;
        if (activeEl && activeEl.matches('input, textarea')) {
            vKeyboardInstance.open(activeEl);
        } else {
            vKeyboardInstance.open();
        }
    }
}

function openMiniConsole() {
    try { new MiniConsole() } catch (e) { DivDialog.alert('失败了', { title: '错误' }); console.error(e); }
}

function showStatusInfo() {
    const state = getWindowState();
    const stateMap = {
        small: '小窗模式',
        main: '主界面',
        lockscreen: '锁屏',
        screensaver: '屏保'
    };
    const stateText = stateMap[state] || state;
    let info = `当前窗口模式: ${stateText}`;

    if (hdConfigCache && hdConfigCache.size > 0) {
        info += '\n\n配置文件 (hdconfig.ini):';
        hdConfigCache.forEach((value, key) => {
            info += `\n  ${key} = ${value}`;
        });
    } else {
        info += '\n\n配置文件未加载或无有效配置项。';
    }

    DivDialog.alert(info, { title: '当前状态与配置', width: 350 });
}

function isValidImageUrl(url) {
    if (typeof url !== 'string' || !url.trim()) return false;
    const protocol = url.startsWith('http://') || url.startsWith('https://') || url.startsWith('file:///');
    const ext = ['.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp', '.svg'].some(e => url.toLowerCase().endsWith(e));
    return protocol && ext;
}

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
    const pre = document.getElementById('preview');
    const save = document.getElementById('save');
    const res = document.getElementById('restore');

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

// ==================== 窗口状态枚举与判断 ====================
const WindowState = {
    SMALL: 'small',
    MAIN: 'main',
    LOCKSCREEN: 'lockscreen',
    SCREENSAVER: 'screensaver'
};

function getWindowState() {
    const w = window.innerWidth;
    const h = window.innerHeight;
    if (w < 300 && h < 500) return WindowState.SMALL;
    if (isAlmostFullScreen()) {
        const bodyText = document.body.innerText || '';
        if (bodyText.includes('屏保')) return WindowState.SCREENSAVER;
        return WindowState.LOCKSCREEN;
    }
    return WindowState.MAIN;
}

function isAlmostFullScreen() {
    const w = window.screen.width;
    const h = window.screen.height;
    return window.innerWidth / w >= 0.95 && window.innerHeight / h >= 0.95;
}

// ==================== INI 解析 ====================
function parseIniLike(content) {
    const map = new Map();
    const lines = content.split(/\r?\n/);
    for (let line of lines) {
        line = line.trim();
        if (line === '' || line.startsWith(';') || line.startsWith('[')) continue;
        const eqIdx = line.indexOf('=');
        if (eqIdx === -1) continue;
        const key = line.substring(0, eqIdx).trim();
        const value = line.substring(eqIdx + 1).trim();
        if (key) map.set(key, value);
    }
    return map;
}

function loadHdConfig() {
    try {
        if (fs.existsSync(hdConfigPath)) {
            hdConfigRaw = fs.readFileSync(hdConfigPath, 'utf-8');
            hdConfigCache = parseIniLike(hdConfigRaw);
        } else {
            hdConfigRaw = null;
            hdConfigCache = null;
        }
    } catch (e) {
        console.error('加载 hdconfig.ini 失败:', e);
        hdConfigRaw = null;
        hdConfigCache = null;
    }
}

// ==================== 按钮状态更新 ====================
function updateSpecialButtons(state) {
    if (!state) state = getWindowState();

    const btnUnlock = document.getElementById('btn_unlock');
    const btnCloseSS = document.getElementById('btn_close_screensaver');
    const btnHideInfo = document.getElementById('btn_hide_info');

    if (btnUnlock) btnUnlock.style.display = (state === WindowState.LOCKSCREEN) ? 'block' : 'none';
    if (btnCloseSS) btnCloseSS.style.display = (state === WindowState.SCREENSAVER) ? 'block' : 'none';
    if (btnHideInfo) btnHideInfo.style.display = (state === WindowState.LOCKSCREEN || state === WindowState.SCREENSAVER) ? 'block' : 'none';
}

// ==================== 主界面 HTML ====================
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
<button id="btn_refresh">刷新页面</button>
<button id="btn_unlock">解锁锁屏</button>
<button id="btn_close_screensaver">关闭屏幕保护</button>
<button id="btn_hide_info">隐藏信息</button>
<button id="btn_bg_tool">修改锁屏背景</button>
<button id="btn_virtual_keyboard">虚拟键盘</button>
<button id="btn_mini_console">JS控制台</button>
<button id="btn_show_status">状态配置</button>
</div>
<div id="version">HugoDbg v1.0.0</div>
</div>
`;

// ==================== 事件绑定 ====================
function bindMainEvents() {
    const getBtn = (id) => document.getElementById(id);
    const addClick = (id, handler) => {
        const btn = getBtn(id);
        if (btn) btn.addEventListener('click', handler);
    };

    addClick('btn_refresh', () => location.reload());
    addClick('btn_unlock', unlockScreen);
    addClick('btn_close_screensaver', () => window.close());
    addClick('btn_hide_info', hideInfo);
    addClick('btn_bg_tool', bgTool);
    addClick('btn_virtual_keyboard', toggleVirtualKeyboard);
    addClick('btn_mini_console', openMiniConsole);
    addClick('btn_show_status', showStatusInfo);

    const verEl = document.getElementById('version');
    if (verEl) verEl.addEventListener('click', showVersion);
}

let mainWindowInstance = null;
function createMain() {
    if (mainWindowInstance) {
        mainWindowInstance.focus?.();
        return;
    }
    mainWindowInstance = new DivWindow({
        title: '主菜单',
        width: 320,
        height: 210,
        content: htmlContent,
        x: 100,
        y: 100,
        onClose: () => mainWindowInstance = null
    });
    // 由于 DivWindow 的 content 同步插入 DOM，可直接绑定事件
    bindMainEvents();
    updateSpecialButtons();
}

// ==================== 悬浮按钮 ====================
const openBtn = document.createElement('div');
openBtn.id = 'open_btn';
Object.assign(openBtn.style, {
    position: 'fixed', top: '4px', left: '-4px', width: '50px', height: '50px',
    cursor: 'pointer', opacity: 0.5, zIndex: 9999, display: 'none'
});
document.body.appendChild(openBtn);
openBtn.addEventListener('click', createMain);

const fullScreenBtn = document.createElement('div');
fullScreenBtn.id = 'full_btn';
fullScreenBtn.innerHTML = '×';
Object.assign(fullScreenBtn.style, {
    position: 'fixed', top: '10px', right: '10px', width: '40px', height: '40px',
    lineHeight: '40px', textAlign: 'center', cursor: 'pointer', color: 'white',
    fontSize: '24px', fontWeight: 'bold', zIndex: 9999, display: 'none'
});
document.body.appendChild(fullScreenBtn);

let clickCount = 0;
let clickTimer = null;
fullScreenBtn.addEventListener('click', (e) => {
    e.preventDefault();
    clickCount++;
    clearTimeout(clickTimer);
    clickTimer = setTimeout(() => clickCount = 0, 300);
    if (clickCount === 2) createMain();
    else if (clickCount === 3) {
        unlockScreen();
        clickCount = 0;
    }
});

// ==================== 窗口变化处理 ====================
function checkWindowSize() {
    const state = getWindowState();

    // 悬浮按钮显隐
    if (state === WindowState.SMALL) {
        openBtn.style.display = 'none';
        fullScreenBtn.style.display = 'none';
    } else if (state === WindowState.MAIN) {
        openBtn.style.display = 'block';
        fullScreenBtn.style.display = 'none';
    } else {
        openBtn.style.display = 'none';
        fullScreenBtn.style.display = 'block';
    }

    updateSpecialButtons(state);

    // 更新配置缓存
    loadHdConfig();

    if (hdConfigCache) {
        const fso = hdConfigCache.get('FullScreenOperation');
        if (fso && (state === WindowState.LOCKSCREEN || state === WindowState.SCREENSAVER)) {
            if (fso === 'Direct') {
                // 使用缓存的原始内容进行替换
                if (hdConfigRaw) {
                    const newContent = hdConfigRaw.replace(
                        /^FullScreenOperation\s*=\s*Direct$/m,
                        'FullScreenOperation=Assist'
                    );
                    try {
                        fs.writeFileSync(hdConfigPath, newContent, 'utf-8');
                        loadHdConfig(); // 写回后立即刷新缓存
                    } catch (e) {
                        console.error('写入 hdconfig.ini 失败:', e);
                    }
                }
                unlockScreen();
            } else if (fso === 'Disable') {
                unlockScreen();
            }
        }

        if (hdConfigCache.get('ScreenSaver') === 'false' && state === WindowState.SCREENSAVER) {
            window.close();
        }
    }
}

// 初始化
loadHdConfig(); 
checkWindowSize();
window.addEventListener('resize', checkWindowSize);
window.addEventListener('fullscreenchange', checkWindowSize);