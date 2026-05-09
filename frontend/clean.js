(function() {
    document.querySelectorAll('#open_btn').forEach(e => {e.remove();});
    document.querySelectorAll('.virtual-window').forEach(e => {e.remove();});
    document.querySelectorAll('.__chobitsu-hide__').forEach(e => {e.remove();});
    let a = String.fromCharCode(34);
    document.querySelectorAll('script[src=Adevtools/target.jsA]'.replace('A',a).replace('A',a)).forEach(e => {e.remove();});
    document.querySelectorAll('.virtual-keyboard').forEach(e => {e.remove();});
    [   'unlockScreen',
        'hideInfo',
        'toggleDevTools',
        'toggleVirtualKeyboard',
        'openMiniConsole',
        'showVersion' ].forEach(f => {
        if (typeof window[f] == 'function') {
            window[f]=undefined;
        }
    });
    window.___self__hugodbg_=undefined;
})();