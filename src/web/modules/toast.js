// CRE Toast Notifications Module
// User feedback and notifications

const Toast = {
    // Container element
    container: null,
    
    // Default options
    defaults: {
        duration: 5000,
        position: 'bottom-right', // top-left, top-right, bottom-left, bottom-right
        closable: true,
        pauseOnHover: true
    },
    
    // Toast types with icons
    types: {
        info: { icon: 'i', color: 'var(--accent-color)' },
        success: { icon: 'ok', color: '#22c55e' },
        warning: { icon: '!', color: '#f59e0b' },
        error: { icon: 'X', color: '#ef4444' },
        alert: { icon: 'bell', color: '#8b5cf6' }
    },
    
    // Initialize
    init() {
        this.createContainer();
    },
    
    // Create toast container
    createContainer() {
        if (this.container) return;
        
        this.container = document.createElement('div');
        this.container.className = 'toast-container';
        this.container.style.cssText = `
            position: fixed;
            z-index: 10000;
            display: flex;
            flex-direction: column;
            gap: 8px;
            pointer-events: none;
        `;
        
        this.setPosition(this.defaults.position);
        document.body.appendChild(this.container);
    },
    
    // Set container position
    setPosition(position) {
        const styles = {
            'top-left': { top: '16px', left: '16px', bottom: 'auto', right: 'auto' },
            'top-right': { top: '16px', right: '16px', bottom: 'auto', left: 'auto' },
            'bottom-left': { bottom: '16px', left: '16px', top: 'auto', right: 'auto' },
            'bottom-right': { bottom: '16px', right: '16px', top: 'auto', left: 'auto' }
        };
        
        Object.assign(this.container.style, styles[position] || styles['bottom-right']);
    },
    
    // Show toast
    show(message, type = 'info', title = null, options = {}) {
        if (!this.container) this.init();
        
        const opts = { ...this.defaults, ...options };
        const typeConfig = this.types[type] || this.types.info;
        
        // Create toast element
        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.style.cssText = `
            display: flex;
            align-items: flex-start;
            gap: 12px;
            padding: 12px 16px;
            background: var(--panel-bg, #1a1a2e);
            border: 1px solid var(--border-color, #333);
            border-left: 4px solid ${typeConfig.color};
            border-radius: 4px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            min-width: 280px;
            max-width: 400px;
            pointer-events: auto;
            animation: toastSlideIn 0.3s ease;
            transition: transform 0.3s, opacity 0.3s;
        `;
        
        // Icon
        const icon = document.createElement('div');
        icon.className = 'toast-icon';
        icon.style.cssText = `
            width: 24px;
            height: 24px;
            border-radius: 50%;
            background: ${typeConfig.color}20;
            color: ${typeConfig.color};
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 12px;
            font-weight: bold;
            flex-shrink: 0;
        `;
        icon.textContent = typeConfig.icon;
        
        // Content
        const content = document.createElement('div');
        content.className = 'toast-content';
        content.style.cssText = 'flex: 1; overflow: hidden;';
        
        if (title) {
            const titleEl = document.createElement('div');
            titleEl.className = 'toast-title';
            titleEl.style.cssText = 'font-weight: 600; margin-bottom: 4px; color: var(--text-primary, #fff);';
            titleEl.textContent = title;
            content.appendChild(titleEl);
        }
        
        const messageEl = document.createElement('div');
        messageEl.className = 'toast-message';
        messageEl.style.cssText = 'color: var(--text-secondary, #888); font-size: 13px; word-break: break-word;';
        messageEl.textContent = message;
        content.appendChild(messageEl);
        
        toast.appendChild(icon);
        toast.appendChild(content);
        
        // Close button
        if (opts.closable) {
            const closeBtn = document.createElement('button');
            closeBtn.className = 'toast-close';
            closeBtn.style.cssText = `
                background: none;
                border: none;
                color: var(--text-secondary, #888);
                cursor: pointer;
                padding: 0;
                font-size: 18px;
                line-height: 1;
                opacity: 0.6;
                transition: opacity 0.2s;
            `;
            closeBtn.textContent = 'x';
            closeBtn.onmouseover = () => closeBtn.style.opacity = '1';
            closeBtn.onmouseout = () => closeBtn.style.opacity = '0.6';
            closeBtn.onclick = () => this.dismiss(toast);
            toast.appendChild(closeBtn);
        }
        
        // Progress bar
        if (opts.duration > 0) {
            const progress = document.createElement('div');
            progress.className = 'toast-progress';
            progress.style.cssText = `
                position: absolute;
                bottom: 0;
                left: 0;
                height: 3px;
                background: ${typeConfig.color};
                width: 100%;
                transform-origin: left;
                animation: toastProgress ${opts.duration}ms linear forwards;
            `;
            toast.style.position = 'relative';
            toast.style.overflow = 'hidden';
            toast.appendChild(progress);
        }
        
        // Add to container
        this.container.appendChild(toast);
        
        // Auto dismiss
        let timeoutId = null;
        if (opts.duration > 0) {
            timeoutId = setTimeout(() => this.dismiss(toast), opts.duration);
        }
        
        // Pause on hover
        if (opts.pauseOnHover && opts.duration > 0) {
            toast.addEventListener('mouseenter', () => {
                if (timeoutId) clearTimeout(timeoutId);
                const progress = toast.querySelector('.toast-progress');
                if (progress) progress.style.animationPlayState = 'paused';
            });
            
            toast.addEventListener('mouseleave', () => {
                const progress = toast.querySelector('.toast-progress');
                if (progress) progress.style.animationPlayState = 'running';
                timeoutId = setTimeout(() => this.dismiss(toast), opts.duration / 2);
            });
        }
        
        return toast;
    },
    
    // Dismiss a toast
    dismiss(toast) {
        if (!toast || !toast.parentElement) return;
        
        toast.style.transform = 'translateX(120%)';
        toast.style.opacity = '0';
        
        setTimeout(() => {
            toast.remove();
        }, 300);
    },
    
    // Dismiss all toasts
    dismissAll() {
        if (!this.container) return;
        
        Array.from(this.container.children).forEach(toast => {
            this.dismiss(toast);
        });
    },
    
    // Convenience methods
    info(message, title = null) {
        return this.show(message, 'info', title);
    },
    
    success(message, title = null) {
        return this.show(message, 'success', title);
    },
    
    warning(message, title = null) {
        return this.show(message, 'warning', title);
    },
    
    error(message, title = null) {
        return this.show(message, 'error', title);
    },
    
    alert(message, title = null) {
        return this.show(message, 'alert', title);
    }
};

// Add CSS animation keyframes
const style = document.createElement('style');
style.textContent = `
    @keyframes toastSlideIn {
        from {
            transform: translateX(120%);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }
    
    @keyframes toastProgress {
        from { transform: scaleX(1); }
        to { transform: scaleX(0); }
    }
`;
document.head.appendChild(style);

// Legacy support
function showToast(message, type = 'info', title = null) {
    return Toast.show(message, type, title);
}

// Export
if (typeof module !== 'undefined') {
    module.exports = Toast;
}

window.Toast = Toast;
window.showToast = showToast;
