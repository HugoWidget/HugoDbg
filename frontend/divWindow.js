class DivWindow {
    static _highestZIndex = 1000;
    static _activeWindow = null;
    
    /**
     * Creates an instance of a DivWindow.
     * @param {object} options - The configuration options for the window.
     * @param {string} [options.title='New Window'] - The title displayed in the window's title bar.
     * @param {number} [options.width=500] - The initial width of the window in pixels.
     * @param {number} [options.height=300] - The initial height of the window in pixels.
     * @param {string|HTMLElement} [options.content=''] - The content of the window. Can be an HTML string or a DOM element.
     * @param {string} [options.backgroundColor='#ffffff'] - The background color of the window's content area.
     * @param {boolean} [options.resizable=true] - Whether the window can be resized by the user.
     * @param {number} [options.minWidth=200] - The minimum width the window can be resized to.
     * @param {number} [options.minHeight=150] - The minimum height the window can be resized to.
     * @param {number} [options.x] - The initial horizontal position of the window. Defaults to center.
     * @param {number} [options.y] - The initial vertical position of the window. Defaults to center.
     * @param {function} [options.onClose=null] - A callback function to execute when the window is closed.
     */
    constructor(options = {}) {
        this.options = {
            title: options.title ?? 'New Window',
            width: options.width ?? 500,
            height: options.height ?? 300,
            content: options.content ?? '',
            backgroundColor: options.backgroundColor ?? '#ffffff',
            resizable: options.resizable ?? true,
            minWidth: options.minWidth ?? 200,
            minHeight: options.minHeight ?? 150,
            x: options.x,
            y: options.y,
            onClose: options.onClose ?? null,
        };

        this.isDragging = false;
        this.isResizing = false;
        
        DivWindow._injectCSS();
        this._createElements();
        this._applyOptions();
        this._attachEventListeners();

        document.body.appendChild(this.windowEl);
        
        this._setInitialPosition();
        this.bringToFront();
    }

    _createElements() {
        this.windowEl = document.createElement('div');
        this.windowEl.className = 'virtual-window';
        this.titleBarEl = document.createElement('div');
        this.titleBarEl.className = 'virtual-window-titlebar';
        this.titleTextEl = document.createElement('span');
        this.titleTextEl.className = 'virtual-window-title';
        this.controlsEl = document.createElement('div');
        this.controlsEl.className = 'virtual-window-controls';
        this.closeBtnEl = document.createElement('button');
        this.closeBtnEl.className = 'virtual-window-close-btn';
        this.closeBtnEl.innerHTML = '×';
        this.contentEl = document.createElement('div');
        this.contentEl.className = 'virtual-window-content';
        
        this.controlsEl.appendChild(this.closeBtnEl);
        this.titleBarEl.appendChild(this.titleTextEl);
        this.titleBarEl.appendChild(this.controlsEl);
        this.windowEl.appendChild(this.titleBarEl);
        this.windowEl.appendChild(this.contentEl);

        if (this.options.resizable) {
            this._createResizeHandles();
        }
    }

    _applyOptions() {
        this.windowEl.style.width = `${this.options.width}px`;
        this.windowEl.style.height = `${this.options.height}px`;
        this.titleTextEl.textContent = this.options.title;
        this.contentEl.style.backgroundColor = this.options.backgroundColor;
        
        if (typeof this.options.content === 'string') {
            this.contentEl.innerHTML = this.options.content;
        } else if (this.options.content instanceof HTMLElement) {
            this.contentEl.appendChild(this.options.content);
        }
    }

    _setInitialPosition() {
        if (this.options.x !== undefined && this.options.y !== undefined) {
            this.windowEl.style.left = `${this.options.x}px`;
            this.windowEl.style.top = `${this.options.y}px`;
        } else {
            const centerX = (window.innerWidth - this.options.width) / 2;
            const centerY = (window.innerHeight - this.options.height) / 2;
            this.windowEl.style.left = `${Math.max(0, centerX)}px`;
            this.windowEl.style.top = `${Math.max(0, centerY)}px`;
        }
    }

    _createResizeHandles() {
        this.windowEl.classList.add('resizable');
        const directions = ['n', 'ne', 'e', 'se', 's', 'sw', 'w', 'nw'];
        directions.forEach(dir => {
            const handle = document.createElement('div');
            handle.className = `resize-handle resize-handle-${dir}`;
            this.windowEl.appendChild(handle);
            
            const onResizeStart = (e) => this._onResizeStart(e, dir);
            handle.addEventListener('mousedown', onResizeStart);
            handle.addEventListener('touchstart', onResizeStart, { passive: false });
        });
    }

    _attachEventListeners() {
        this.closeBtnEl.addEventListener('click', () => this.close());
        this.windowEl.addEventListener('mousedown', () => this.bringToFront());
        this.windowEl.addEventListener('touchstart', () => this.bringToFront(), { passive: true });
        this.titleBarEl.addEventListener('mousedown', this._onDragStart.bind(this));
        this.titleBarEl.addEventListener('touchstart', this._onDragStart.bind(this), { passive: false });
    }

    bringToFront() {
        if (DivWindow._activeWindow === this) return;
        if (DivWindow._activeWindow) {
            DivWindow._activeWindow.windowEl.classList.remove('is-active');
        }
        this.windowEl.style.zIndex = ++DivWindow._highestZIndex;
        this.windowEl.classList.add('is-active');
        DivWindow._activeWindow = this;
    }

    _onDragStart(event) {
        if (event.target.closest('.virtual-window-close-btn')) {
            return;
        }
        if (this.isResizing) return;
        event.preventDefault();
        this.isDragging = true;
        this.windowEl.classList.add('is-dragging');
        const rect = this.windowEl.getBoundingClientRect();
        const eventClientX = event.type === 'touchstart' ? event.touches[0].clientX : event.clientX;
        const eventClientY = event.type === 'touchstart' ? event.touches[0].clientY : event.clientY;
        this.dragOffsetX = eventClientX - rect.left;
        this.dragOffsetY = eventClientY - rect.top;
        this._dragMoveHandler = this._onDragMove.bind(this);
        this._dragEndHandler = this._onDragEnd.bind(this);
        document.addEventListener('mousemove', this._dragMoveHandler);
        document.addEventListener('mouseup', this._dragEndHandler);
        document.addEventListener('touchmove', this._dragMoveHandler, { passive: false });
        document.addEventListener('touchend', this._dragEndHandler);
    }

    _onDragMove(event) {
        if (!this.isDragging) return;
        if (event.type === 'touchmove') event.preventDefault();
        const eventClientX = event.type === 'touchmove' ? event.touches[0].clientX : event.clientX;
        const eventClientY = event.type === 'touchmove' ? event.touches[0].clientY : event.clientY;
        let newX = eventClientX - this.dragOffsetX;
        let newY = eventClientY - this.dragOffsetY;
        const maxWidth = window.innerWidth - this.windowEl.offsetWidth;
        const maxHeight = window.innerHeight - this.windowEl.offsetHeight;
        newX = Math.max(0, Math.min(newX, maxWidth));
        newY = Math.max(0, Math.min(newY, maxHeight));
        this.windowEl.style.left = `${newX}px`;
        this.windowEl.style.top = `${newY}px`;
    }

    _onDragEnd() {
        this.isDragging = false;
        this.windowEl.classList.remove('is-dragging');
        document.removeEventListener('mousemove', this._dragMoveHandler);
        document.removeEventListener('mouseup', this._dragEndHandler);
        document.removeEventListener('touchmove', this._dragMoveHandler);
        document.removeEventListener('touchend', this._dragEndHandler);
    }

    _onResizeStart(event, direction) {
        event.preventDefault();
        event.stopPropagation();
        this.isResizing = true;
        this.resizeDirection = direction;
        const rect = this.windowEl.getBoundingClientRect();
        this.initialRect = { width: rect.width, height: rect.height, left: rect.left, top: rect.top };
        const eventClientX = event.type === 'touchstart' ? event.touches[0].clientX : event.clientX;
        const eventClientY = event.type === 'touchstart' ? event.touches[0].clientY : event.clientY;
        this.initialMousePos = { x: eventClientX, y: eventClientY };
        this._resizeMoveHandler = this._onResizeMove.bind(this);
        this._resizeEndHandler = this._onResizeEnd.bind(this);
        document.addEventListener('mousemove', this._resizeMoveHandler);
        document.addEventListener('mouseup', this._resizeEndHandler);
        document.addEventListener('touchmove', this._resizeMoveHandler, { passive: false });
        document.addEventListener('touchend', this._resizeEndHandler);
    }

    _onResizeMove(event) {
        if (!this.isResizing) return;
        if (event.type === 'touchmove') event.preventDefault();
        const eventClientX = event.type === 'touchmove' ? event.touches[0].clientX : event.clientX;
        const eventClientY = event.type === 'touchmove' ? event.touches[0].clientY : event.clientY;
        const dx = eventClientX - this.initialMousePos.x;
        const dy = eventClientY - this.initialMousePos.y;
        let newWidth = this.initialRect.width, newHeight = this.initialRect.height;
        let newLeft = this.initialRect.left, newTop = this.initialRect.top;
        if (this.resizeDirection.includes('e')) newWidth = this.initialRect.width + dx;
        if (this.resizeDirection.includes('w')) { newWidth = this.initialRect.width - dx; newLeft = this.initialRect.left + dx; }
        if (this.resizeDirection.includes('s')) newHeight = this.initialRect.height + dy;
        if (this.resizeDirection.includes('n')) { newHeight = this.initialRect.height - dy; newTop = this.initialRect.top + dy; }
        if (newWidth < this.options.minWidth) {
            newWidth = this.options.minWidth;
            if (this.resizeDirection.includes('w')) newLeft = this.initialRect.left + (this.initialRect.width - newWidth);
        }
        if (newHeight < this.options.minHeight) {
            newHeight = this.options.minHeight;
            if (this.resizeDirection.includes('n')) newTop = this.initialRect.top + (this.initialRect.height - newHeight);
        }
        this.windowEl.style.width = `${newWidth}px`;
        this.windowEl.style.height = `${newHeight}px`;
        this.windowEl.style.left = `${newLeft}px`;
        this.windowEl.style.top = `${newTop}px`;
    }

    _onResizeEnd() {
        this.isResizing = false;
        document.removeEventListener('mousemove', this._resizeMoveHandler);
        document.removeEventListener('mouseup', this._resizeEndHandler);
        document.removeEventListener('touchmove', this._resizeMoveHandler);
        document.removeEventListener('touchend', this._resizeEndHandler);
    }
    
    close() {
        if (this.options.onClose && typeof this.options.onClose === 'function') {
            this.options.onClose();
        }
        this.windowEl.remove();
        if (DivWindow._activeWindow === this) {
            DivWindow._activeWindow = null;
        }
    }
    
    static _injectCSS() {
        const styleId = 'virtual-window-styles';
        if (document.getElementById(styleId)) return;
        const css = `
.virtual-window {
position: fixed;
border: none;
background-color: transparent;
box-shadow: 0 2px 4px rgba(0, 0, 0, 0.25);
display: flex;
flex-direction: column;
font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
border-radius: 0px;
min-width: 200px;
min-height: 150px;
}
.virtual-window.is-active {
box-shadow: 0 6px 20px rgba(0, 0, 0, 0.3);
}
.virtual-window.is-dragging, .virtual-window.is-resizing {
user-select: none; opacity: 0.85;
}
.virtual-window-titlebar {
display: flex; justify-content: space-between; align-items: center;
height: 32px; padding-left: 12px;
background-color: rgb(26, 144, 255);
color: #fff;
border-bottom: 1px solid rgba(0,0,0,0.1);
cursor: move; flex-shrink: 0;
border-top-left-radius: 4px; border-top-right-radius: 4px;
}
.virtual-window.is-active .virtual-window-titlebar {
background-color: rgb(26, 144, 255);
}
.virtual-window-title {
font-size: 14px; color: #fff; white-space: nowrap;
overflow: hidden; text-overflow: ellipsis; padding-right: 10px;
}
.virtual-window-controls { display: flex; align-items: center; flex-shrink: 0; }
.virtual-window-close-btn {
width: 46px; height: 32px; border: none; background-color: transparent;
font-size: 20px; color: #ffffff; cursor: pointer; line-height: 32px;
text-align: center; padding: 0; transition: background-color 0.15s ease;
border-top-right-radius: 4px;
}
.virtual-window-close-btn:hover { background-color: #e81123; color: #ffffff; }
.virtual-window-close-btn:active { background-color: #f1707a; color: #ffffff; }
.virtual-window-content {
flex-grow: 1; padding: 0; overflow: hidden;
background-color: #ffffff;
border-bottom-left-radius: 3px; border-bottom-right-radius: 3px;
}
.resize-handle {
position: absolute; background: transparent; z-index: 10;
}
.resize-handle-n { top: -7px; left: 0; right: 0; height: 14px; cursor: n-resize; }
.resize-handle-s { bottom: -7px; left: 0; right: 0; height: 14px; cursor: s-resize; }
.resize-handle-e { top: 0; right: -7px; bottom: 0; width: 14px; cursor: e-resize; }
.resize-handle-w { top: 0; left: -7px; bottom: 0; width: 14px; cursor: w-resize; }
.resize-handle-ne { top: -7px; right: -7px; width: 14px; height: 14px; cursor: ne-resize; }
.resize-handle-nw { top: -7px; left: -7px; width: 14px; height: 14px; cursor: nw-resize; }
.resize-handle-se { bottom: -7px; right: -7px; width: 14px; height: 14px; cursor: se-resize; }
.resize-handle-sw { bottom: -7px; left: -7px; width: 14px; height: 14px; cursor: sw-resize; }
`;
        const style = document.createElement('style');
        style.id = styleId;
        style.type = 'text/css';
        style.appendChild(document.createTextNode(css));
        document.head.appendChild(style);
    }
}