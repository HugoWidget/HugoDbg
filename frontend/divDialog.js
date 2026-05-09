class DivDialog {
    /**
     * Shows an alert dialog with a single "OK" button.
     * @param {string} message - The message to display in the dialog.
     * @param {object} [options] - Custom options for the dialog window.
     * @param {string} [options.title='提示'] - The title of the dialog.
     * @param {string} [options.okText='确认'] - The text for the OK button.
     * @returns {Promise<boolean>} A promise that resolves to true when the dialog is closed.
     */
    static alert(message, options = {}) {
        return new Promise((resolve) => {
            const { title = '提示', okText = '确认', ...windowOptions } = options;
            const { contentEl, dialogWindow } = this._createDialogBase(
                message, title, () => resolve(true), windowOptions
            );
            const okBtn = this._createButton(okText, 'primary');
            okBtn.addEventListener('click', () => dialogWindow.close());
            const buttonContainer = contentEl.querySelector('.div-dialog-buttons');
            buttonContainer.appendChild(okBtn);
        });
    }

    /**
     * Shows a confirmation dialog with "OK" and "Cancel" buttons.
     * @param {string} message - The message to display in the dialog.
     * @param {object} [options] - Custom options for the dialog window.
     * @param {string} [options.title='确认'] - The title of the dialog.
     * @param {string} [options.okText='确认'] - The text for the OK button.
     * @param {string} [options.cancelText='取消'] - The text for the Cancel button.
     * @returns {Promise<boolean>} A promise that resolves to true if "OK" is clicked, and false otherwise.
     */
    static confirm(message, options = {}) {
        return new Promise((resolve) => {
            const { title = '确认', okText = '确认', cancelText = '取消', ...windowOptions } = options;
            let result = false;
            const { contentEl, dialogWindow } = this._createDialogBase(
                message, title, () => resolve(result), windowOptions
            );
            const okBtn = this._createButton(okText, 'primary');
            okBtn.addEventListener('click', () => { result = true; dialogWindow.close(); });
            const cancelBtn = this._createButton(cancelText);
            cancelBtn.addEventListener('click', () => { result = false; dialogWindow.close(); });
            const buttonContainer = contentEl.querySelector('.div-dialog-buttons');
            buttonContainer.appendChild(cancelBtn);
            buttonContainer.appendChild(okBtn);
        });
    }

    /**
     * @private
     * 内部方法，用于创建对话框的基础结构 (内容和窗口)
     */
    static _createDialogBase(message, title, onClose, windowOptions) {
        if (typeof DivWindow === 'undefined') {
            throw new Error('DivDialog requires DivWindow library to be loaded first.');
        }
        
        this._injectCSS();

        const contentEl = document.createElement('div');
        contentEl.className = 'div-dialog-content';
        contentEl.innerHTML = `
            <p class="div-dialog-message"></p>
            <div class="div-dialog-buttons"></div>
        `;
        contentEl.querySelector('.div-dialog-message').textContent = message;

        const defaultWinOptions = {
            width: 320,
            height: 160,
            resizable: false,
        };

        const dialogWindow = new DivWindow({
            title: title,
            content: contentEl,
            onClose: onClose,
            ...defaultWinOptions,
            ...windowOptions,
        });

        return { contentEl, dialogWindow };
    }

    /**
     * @private
     * 内部方法，用于创建标准化的按钮
     */
    static _createButton(text, type = 'default') {
        const btn = document.createElement('button');
        btn.className = `div-dialog-btn div-dialog-btn-${type}`;
        btn.textContent = text;
        return btn;
    }

    /**
     * @private
     * 注入对话框所需的特定CSS
     */
    static _injectCSS() {
        const styleId = 'div-dialog-styles';
        if (document.getElementById(styleId)) return;

        const css = `
.div-dialog-content {
display: flex;
flex-direction: column;
justify-content: space-between;
height: 100%;
box-sizing: border-box;
}
.div-dialog-message {
font-size: 14px;
color: #333;
margin: 15px 15px 15px;
flex-grow: 1;
line-height: 1.5;
}
.div-dialog-buttons {
display: flex;
justify-content: flex-end;
gap: 8px;
padding: 4px 12px;
border-top: 1px solid #eee;
background-color: #f9f9f9;
}
.div-dialog-btn {
min-width: 70px;
padding: 6px 12px;
border: none;
border-radius: 2px;
background-color: rgb(8, 209, 82);
color: #fff;
cursor: pointer;
font-size: 14px;
transition: all 0.2s ease;
}
.div-dialog-btn:hover {
background-color: rgb(6, 180, 68);
}
.div-dialog-btn-primary {
background-color: rgb(8, 209, 82);
color: white;
}
.div-dialog-btn-primary:hover {
background-color: rgb(6, 180, 68);
}
`;

        const style = document.createElement('style');
        style.id = styleId;
        style.type = 'text/css';
        style.appendChild(document.createTextNode(css));
        document.head.appendChild(style);
    }
}