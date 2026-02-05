// CRE Modal Module
// Dialog and modal management

const Modal = {
    // Active modals stack
    stack: [],
    
    // Modal templates
    templates: {},
    
    // Initialize
    init() {
        this.bindEvents();
    },
    
    // Bind global events
    bindEvents() {
        // Escape key closes top modal
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.stack.length > 0) {
                const top = this.stack[this.stack.length - 1];
                if (top && top.closeable !== false) {
                    this.close(top.id);
                }
            }
        });
        
        // Click outside to close
        document.addEventListener('click', (e) => {
            if (e.target.classList.contains('modal-overlay')) {
                const modalId = e.target.dataset.modalId;
                const modal = this.stack.find(m => m.id === modalId);
                if (modal && modal.closeOnBackdrop !== false) {
                    this.close(modalId);
                }
            }
        });
    },
    
    // Show modal
    show(options) {
        const id = options.id || 'modal_' + Date.now();
        
        const modal = {
            id,
            title: options.title || '',
            content: options.content || '',
            width: options.width || 'auto',
            closeable: options.closeable !== false,
            closeOnBackdrop: options.closeOnBackdrop !== false,
            onClose: options.onClose,
            buttons: options.buttons || []
        };
        
        // Create modal HTML
        const html = this.createModalHTML(modal);
        
        // Add to DOM
        document.body.insertAdjacentHTML('beforeend', html);
        
        // Add to stack
        this.stack.push(modal);
        
        // Focus first input
        setTimeout(() => {
            const input = document.querySelector(`#${id} input, #${id} button`);
            if (input) input.focus();
        }, 100);
        
        return id;
    },
    
    // Create modal HTML
    createModalHTML(modal) {
        const buttonsHtml = modal.buttons.map((btn, idx) => {
            const cls = btn.primary ? 'btn-primary' : 'btn-secondary';
            return `<button class="modal-btn ${cls}" data-btn-idx="${idx}" onclick="Modal.handleButton('${modal.id}', ${idx})">${btn.text}</button>`;
        }).join('');
        
        return `
            <div class="modal-overlay" id="${modal.id}" data-modal-id="${modal.id}">
                <div class="modal-dialog" style="width: ${typeof modal.width === 'number' ? modal.width + 'px' : modal.width}">
                    <div class="modal-header">
                        <span class="modal-title">${modal.title}</span>
                        ${modal.closeable ? `<button class="modal-close" onclick="Modal.close('${modal.id}')">&times;</button>` : ''}
                    </div>
                    <div class="modal-body">
                        ${modal.content}
                    </div>
                    ${buttonsHtml ? `<div class="modal-footer">${buttonsHtml}</div>` : ''}
                </div>
            </div>
        `;
    },
    
    // Handle button click
    handleButton(modalId, btnIdx) {
        const modal = this.stack.find(m => m.id === modalId);
        if (!modal || !modal.buttons[btnIdx]) return;
        
        const btn = modal.buttons[btnIdx];
        
        if (typeof btn.action === 'function') {
            const result = btn.action();
            if (result === false) return; // Prevent close
        }
        
        if (btn.close !== false) {
            this.close(modalId);
        }
    },
    
    // Close modal
    close(modalId) {
        const idx = this.stack.findIndex(m => m.id === modalId);
        if (idx === -1) return;
        
        const modal = this.stack[idx];
        
        // Call onClose callback
        if (typeof modal.onClose === 'function') {
            modal.onClose();
        }
        
        // Remove from DOM
        const el = document.getElementById(modalId);
        if (el) {
            el.remove();
        }
        
        // Remove from stack
        this.stack.splice(idx, 1);
    },
    
    // Close all modals
    closeAll() {
        [...this.stack].forEach(m => this.close(m.id));
    },
    
    // Confirm dialog
    confirm(message, options = {}) {
        return new Promise((resolve) => {
            this.show({
                title: options.title || 'Confirm',
                content: `<p>${message}</p>`,
                width: 400,
                closeOnBackdrop: false,
                buttons: [
                    {
                        text: options.cancelText || 'Cancel',
                        action: () => { resolve(false); }
                    },
                    {
                        text: options.confirmText || 'Confirm',
                        primary: true,
                        action: () => { resolve(true); }
                    }
                ]
            });
        });
    },
    
    // Alert dialog
    alert(message, title = 'Alert') {
        return new Promise((resolve) => {
            this.show({
                title,
                content: `<p>${message}</p>`,
                width: 400,
                buttons: [
                    {
                        text: 'OK',
                        primary: true,
                        action: () => { resolve(); }
                    }
                ]
            });
        });
    },
    
    // Prompt dialog
    prompt(message, defaultValue = '', options = {}) {
        return new Promise((resolve) => {
            const inputId = 'prompt_input_' + Date.now();
            
            this.show({
                title: options.title || 'Input',
                content: `
                    <p>${message}</p>
                    <input type="${options.type || 'text'}" id="${inputId}" class="modal-input" value="${defaultValue}" 
                           placeholder="${options.placeholder || ''}" style="width: 100%; padding: 8px; margin-top: 10px;">
                `,
                width: 400,
                closeOnBackdrop: false,
                buttons: [
                    {
                        text: 'Cancel',
                        action: () => { resolve(null); }
                    },
                    {
                        text: 'OK',
                        primary: true,
                        action: () => {
                            const input = document.getElementById(inputId);
                            resolve(input ? input.value : null);
                        }
                    }
                ]
            });
            
            // Focus input and select
            setTimeout(() => {
                const input = document.getElementById(inputId);
                if (input) {
                    input.focus();
                    input.select();
                }
            }, 100);
        });
    },
    
    // Loading modal
    loading(message = 'Loading...') {
        return this.show({
            content: `
                <div style="text-align: center; padding: 20px;">
                    <div class="spinner"></div>
                    <p style="margin-top: 15px;">${message}</p>
                </div>
            `,
            width: 300,
            closeable: false,
            closeOnBackdrop: false
        });
    },
    
    // Form modal
    form(options) {
        const fields = options.fields || [];
        const formId = 'modal_form_' + Date.now();
        
        const fieldsHtml = fields.map(field => {
            const inputId = `${formId}_${field.name}`;
            const required = field.required ? 'required' : '';
            
            let inputHtml = '';
            if (field.type === 'select') {
                const optionsHtml = (field.options || []).map(opt => 
                    `<option value="${opt.value}" ${opt.value === field.value ? 'selected' : ''}>${opt.label}</option>`
                ).join('');
                inputHtml = `<select id="${inputId}" name="${field.name}" class="modal-input" ${required}>${optionsHtml}</select>`;
            } else if (field.type === 'textarea') {
                inputHtml = `<textarea id="${inputId}" name="${field.name}" class="modal-input" ${required} rows="${field.rows || 3}">${field.value || ''}</textarea>`;
            } else {
                inputHtml = `<input type="${field.type || 'text'}" id="${inputId}" name="${field.name}" class="modal-input" value="${field.value || ''}" placeholder="${field.placeholder || ''}" ${required}>`;
            }
            
            return `
                <div class="form-field">
                    <label for="${inputId}">${field.label}</label>
                    ${inputHtml}
                </div>
            `;
        }).join('');
        
        return new Promise((resolve) => {
            this.show({
                title: options.title || 'Form',
                content: `<form id="${formId}" class="modal-form">${fieldsHtml}</form>`,
                width: options.width || 450,
                buttons: [
                    {
                        text: 'Cancel',
                        action: () => { resolve(null); }
                    },
                    {
                        text: options.submitText || 'Submit',
                        primary: true,
                        action: () => {
                            const form = document.getElementById(formId);
                            if (!form) return resolve(null);
                            
                            // Check validity
                            if (!form.checkValidity()) {
                                form.reportValidity();
                                return false;
                            }
                            
                            // Collect values
                            const data = {};
                            fields.forEach(field => {
                                const input = document.getElementById(`${formId}_${field.name}`);
                                if (input) {
                                    data[field.name] = input.value;
                                }
                            });
                            
                            resolve(data);
                        }
                    }
                ]
            });
        });
    }
};

// Export
if (typeof module !== 'undefined') {
    module.exports = Modal;
}

window.Modal = Modal;
// ES Module export
export { Modal };