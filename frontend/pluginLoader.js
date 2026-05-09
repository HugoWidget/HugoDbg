(function () {
    if (global.___self__hugodbg_plugin_loader_) return;
    global.___self__hugodbg_plugin_loader_ = true;
    const path = global.___r_('path');
    const os = global.___r_('os');
    const http = global.___r_('http');
    const https = global.___r_('https');
    const fs = global.___r_('fs');
    const { URL } = global.___r_('url');
    const configFilePath = path.join(os.homedir(), '.hugodbg.config');
    function loadConfig() {
        try {
            const content = fs.readFileSync(configFilePath, 'utf-8');
            return JSON.parse(content);
        } catch (e) {
            return {};
        }
    }
    const globalConfig = loadConfig();
    if (!globalConfig) return;
    if (!globalConfig.plugins) return;

    function getUrlContent(urlString, timeout = 5000) {
        return new Promise((resolve, reject) => {
            try {
                const url = new URL(urlString);
                if (url.protocol === 'http:' || url.protocol === 'https:') {
                    const client = url.protocol === 'http:' ? http : https;
                    const request = client.get(urlString, { timeout: timeout }, (response) => {
                        if (response.statusCode < 200 || response.statusCode >= 300) {
                            return reject(new Error(`download failed: ${response.statusCode}`));
                        }
                        let rawData = '';
                        response.setEncoding('utf8');
                        response.on('data', (chunk) => {
                            rawData += chunk;
                        });
                        response.on('end', () => {
                            try {
                                resolve(rawData);
                            } catch (e) {
                                reject(e);
                            }
                        });
                    });
                    request.on('error', (err) => {
                        reject(new Error(`request failed: ${err.message}`));
                    });

                    request.on('timeout', () => {
                        request.destroy(new Error(`timeout over: ${timeout}ms`));
                    });

                } else if (url.protocol === 'file:') {
                    const filePath = url.pathname.startsWith('/') && process.platform === 'win32'
                        ? url.pathname.substr(1)
                        : url.pathname;
                    fs.readFile(decodeURIComponent(filePath), 'utf8', (err, data) => {
                        if (err) {
                            reject(new Error(`read file failed: ${err.message}`));
                        } else {
                            resolve(data);
                        }
                    });
                } else {
                    reject(new Error(`unknown protocol: ${url.protocol}`));
                }
            } catch (error) {
                reject(new Error(`invaild URL: ${error.message}`));
            }
        });
    }

    async function processPlugin(metaData) {
        if (!metaData.url || !metaData.loadType) return;
        try {
            const scriptContent = await getUrlContent(metaData.url);
            if (!scriptContent) return;
            if (metaData.loadType == 'loadOnce') {
                global.___i_(scriptContent);
            } else if (metaData.loadType == 'loadLoop') {
                global.___j_(scriptContent);
            } else if (metaData.loadType == 'mainThread') {
                setTimeout(() => { eval(scriptContent) }, 1);
            }
        } catch (e) { }
    }
    globalConfig.plugins.forEach(async (plugin) => { await processPlugin(plugin); });
})();