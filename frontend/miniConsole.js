class MiniConsole {
    constructor() {
        this.history = [];
        this.historyIndex = -1;

        this._injectConsoleCSS();
        this._createConsoleElement();
        this._attachEventListeners();
        this._createWindow();
        this._logWelcomeMessage();
    }

    _injectConsoleCSS() {
        const styleId = 'mini-console-styles';
        if (document.getElementById(styleId)) return;
        const css = `
.mini-console-container {
display: flex;
flex-direction: column;
height: 100%;
font-family: Menlo, Monaco, Consolas, "Courier New", monospace;
font-size: 14px;
background-color: #ffffff;
}
.mini-console-output {
flex-grow: 1;
overflow-y: auto;
padding: 10px;
border-bottom: 1px solid #e0e0e0;
color: #333;
}
.mini-console-output .log-line {
display: flex;
white-space: pre-wrap;
word-break: break-all;
border-bottom: 1px solid #f0f0f0;
padding: 4px 0;
align-items: flex-start;
}
.mini-console-output .log-line:last-child {
border-bottom: none;
}
.mini-console-output .log-icon {
flex-shrink: 0;
margin-right: 8px;
color: #999;
line-height: 1.5;
}
.mini-console-output .log-content {
flex-grow: 1;
margin: 0;
line-height: 1.5;
}
.log-input .log-content { color: #555; }
.log-output .log-content { color: #00008b; /* DarkBlue */ }
.log-output .log-content.undefined { color: #999; }
.log-error .log-content { color: #d32f2f; }
.log-info .log-content { color: #666; font-style: italic; }
.mini-console-input-area {
display: flex;
align-items: center;
padding: 5px 10px;
flex-shrink: 0;
}
.mini-console-prompt {
color: #0078d7;
margin-right: 8px;
}
.mini-console-input {
flex-grow: 1;
border: none;
outline: none;
background-color: transparent;
font-family: inherit;
font-size: inherit;
}
        `;
        const style = document.createElement('style');
        style.id = styleId;
        style.type = 'text/css';
        style.appendChild(document.createTextNode(css));
        document.head.appendChild(style);
    }

    _createConsoleElement() {
        this.containerEl = document.createElement('div');
        this.containerEl.className = 'mini-console-container';

        this.outputEl = document.createElement('div');
        this.outputEl.className = 'mini-console-output';

        const inputAreaEl = document.createElement('div');
        inputAreaEl.className = 'mini-console-input-area';

        const promptEl = document.createElement('span');
        promptEl.className = 'mini-console-prompt';
        promptEl.textContent = '>';

        this.inputEl = document.createElement('input');
        this.inputEl.className = 'mini-console-input';
        this.inputEl.type = 'text';
        this.inputEl.placeholder = 'Type JavaScript here';

        inputAreaEl.appendChild(promptEl);
        inputAreaEl.appendChild(this.inputEl);

        this.containerEl.appendChild(this.outputEl);
        this.containerEl.appendChild(inputAreaEl);
    }
    
    _attachEventListeners() {
        this.inputEl.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                const code = this.inputEl.value.trim();
                if (code) {
                    this.history.push(code);
                    this.historyIndex = this.history.length;
                    this.execute(code);
                }
                this.inputEl.value = '';
            } else if (e.key === 'ArrowUp') {
                 e.preventDefault();
                 if(this.historyIndex > 0) {
                     this.historyIndex--;
                     this.inputEl.value = this.history[this.historyIndex];
                     this.inputEl.selectionStart = this.inputEl.selectionEnd = this.inputEl.value.length;
                 }
            } else if (e.key === 'ArrowDown') {
                 e.preventDefault();
                 if(this.historyIndex < this.history.length - 1) {
                     this.historyIndex++;
                     this.inputEl.value = this.history[this.historyIndex];
                     this.inputEl.selectionStart = this.inputEl.selectionEnd = this.inputEl.value.length;
                 } else if (this.historyIndex === this.history.length - 1) {
                     this.historyIndex++;
                     this.inputEl.value = "";
                 }
            }
        });

        this.containerEl.addEventListener('click', (e) => {
            if (window.getSelection().toString() === '') {
                 this.inputEl.focus();
            }
        });
    }

    _createWindow() {
        this.win = new DivWindow({
            title: 'Mini DevTools Console',
            width: 400,
            height: 300,
            minWidth: 250,
            minHeight: 150,
            content: this.containerEl,
            backgroundColor: 'transparent'
        });
    }

    _logWelcomeMessage() {
        this.log('info', 'Welcome to Mini DevTools Console!');
        this.log('info', 'Use ArrowUp/ArrowDown to navigate history. Try `new Date()`');
    }

    execute(code) {
        this.log('input', code);
        try {
            const result = new Function(`return ${code}`)();
            this.log('output', result);
        } catch (error) {
            this.log('error', error);
        }
    }

    log(type, content) {
        const lineEl = document.createElement('div');
        lineEl.className = `log-line log-${type}`;

        const iconEl = document.createElement('span');
        iconEl.className = 'log-icon';
        
        const contentEl = document.createElement('pre');
        contentEl.className = 'log-content';
        
        switch (type) {
            case 'input':
                iconEl.textContent = '>';
                contentEl.textContent = content;
                break;
            case 'output':
                iconEl.textContent = '<';
                contentEl.textContent = this._formatOutput(content);
                if(content === undefined) {
                    contentEl.classList.add('undefined');
                }
                break;
            case 'error':
                iconEl.textContent = '×';
                contentEl.textContent = `Uncaught ${content.name}: ${content.message}`;
                break;
            case 'info':
                iconEl.textContent = 'i';
                contentEl.textContent = content;
                break;
        }

        lineEl.appendChild(iconEl);
        lineEl.appendChild(contentEl);
        this.outputEl.appendChild(lineEl);

        this.outputEl.scrollTop = this.outputEl.scrollHeight;
    }

    _formatOutput(value) {
        if (value === undefined) return 'undefined';
        if (value === null) return 'null';
        if (typeof value === 'string') return `"${value}"`;
        if (typeof value === 'object' || Array.isArray(value)) {
            try {
                return JSON.stringify(value, null, 2);
            } catch (e) {
                return String(value);
            }
        }
        return String(value);
    }
}