/**
 * CRE.mn Trading Platform — Authentication Module
 * ==============================================
 * OTP-based phone authentication system
 */

import { API, toast } from './utils.js';

/**
 * Send OTP code to phone
 */
export async function sendCode(state, dom) {
    const phone = dom.phoneInput.value.replace(/\D/g, '');
    if (phone.length !== 8) { 
        toast('Enter valid 8-digit number', 'error', state, dom); 
        return; 
    }

    dom.sendCodeBtn.disabled = true;
    dom.sendCodeBtn.textContent = 'Sending…';

    try {
        const res = await fetch(`${API}/auth/request-otp`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ phone: '+976' + phone }),
        });
        const data = await res.json();
        
        if (data.success) {
            dom.stepPhone.classList.remove('active');
            dom.stepOtp.classList.add('active');
            const firstDigit = dom.stepOtp.querySelector('.otp-digit');
            if (firstDigit) firstDigit.focus();
            toast('Code sent!', 'success', state, dom);
        } else {
            toast(data.error || 'Failed to send code', 'error', state, dom);
        }
    } catch (err) {
        toast('Network error', 'error', state, dom);
        console.error('[Auth] Send code error:', err);
    } finally {
        dom.sendCodeBtn.disabled = false;
        dom.sendCodeBtn.textContent = 'Send Verification Code';
    }
}

/**
 * Verify OTP code and authenticate
 */
export async function verifyCode(state, dom) {
    const phone = dom.phoneInput.value.replace(/\D/g, '');
    const digits = dom.stepOtp.querySelectorAll('.otp-digit');
    const otp = Array.from(digits).map(d => d.value).join('');
    
    if (otp.length !== 6) { 
        toast('Enter 6-digit code', 'error', state, dom); 
        return; 
    }

    dom.verifyBtn.disabled = true;
    
    try {
        const res = await fetch(`${API}/auth/verify-otp`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ phone: '+976' + phone, otp: otp }),
        });
        const data = await res.json();
        
        if (data.success && data.token) {
            // Store token and update UI
            state.token = data.token;
            localStorage.setItem('cre_token', data.token);
            
            // Update login button
            dom.loginBtn.textContent = 'Connected';
            dom.loginBtn.classList.add('connected');
            dom.balancePill.classList.remove('hidden');
            
            // Hide modal
            dom.modalBackdrop.classList.add('hidden');
            
            // Reset form
            resetAuthForm(dom);
            
            toast('Welcome to CRE!', 'success', state, dom);
            
            // Reconnect SSE with token
            if (window.connectSSE) {
                window.connectSSE(state, dom);
            }
            
            // Fetch account data
            if (window.fetchAccount) {
                window.fetchAccount();
            }
            
        } else {
            toast(data.error || 'Invalid code', 'error', state, dom);
        }
    } catch (err) {
        toast('Network error', 'error', state, dom);
        console.error('[Auth] Verify code error:', err);
    } finally {
        dom.verifyBtn.disabled = false;
    }
}

/**
 * Logout user
 */
export function logout(state, dom) {
    // Clear token
    state.token = null;
    localStorage.removeItem('cre_token');
    
    // Update UI
    dom.loginBtn.textContent = 'Login';
    dom.loginBtn.classList.remove('connected');
    dom.balancePill.classList.add('hidden');
    
    // Clear sensitive data
    state.account = null;
    state.positions = [];
    state.orders = [];
    state.trades = [];
    
    // Reset views
    if (window.renderPositions) window.renderPositions();
    if (window.renderOrders) window.renderOrders();
    
    toast('Logged out', 'info', state, dom);
}

/**
 * Check if user is authenticated
 */
export function isAuthenticated(state) {
    return !!state.token;
}

/**
 * Handle session expiry (called from apiFetch on 401)
 */
export function handleSessionExpiry(state, dom) {
    logout(state, dom);
    toast('Session expired — please login again', 'error', state, dom);
    dom.modalBackdrop.classList.remove('hidden');
}

/**
 * Reset authentication form
 */
function resetAuthForm(dom) {
    // Reset phone step
    dom.phoneInput.value = '';
    dom.stepPhone.classList.add('active');
    dom.stepOtp.classList.remove('active');
    
    // Clear OTP digits
    const digits = dom.stepOtp.querySelectorAll('.otp-digit');
    digits.forEach(digit => digit.value = '');
    
    // Reset button states
    dom.sendCodeBtn.disabled = false;
    dom.sendCodeBtn.textContent = 'Send Verification Code';
    dom.verifyBtn.disabled = false;
}

/**
 * Show login modal
 */
export function showLogin(dom) {
    resetAuthForm(dom);
    dom.modalBackdrop.classList.remove('hidden');
    setTimeout(() => dom.phoneInput.focus(), 100);
}

/**
 * Hide login modal
 */
export function hideLogin(dom) {
    dom.modalBackdrop.classList.add('hidden');
}

/**
 * Setup OTP digit input handlers
 */
function setupOTPDigits(dom) {
    const digits = dom.stepOtp.querySelectorAll('.otp-digit');
    
    digits.forEach((digit, i) => {
        // Move to next digit on input
        digit.addEventListener('input', (e) => {
            if (e.target.value.length === 1 && i < digits.length - 1) {
                digits[i + 1].focus();
            }
            
            // Auto-verify when all digits filled
            if (i === digits.length - 1 && e.target.value.length === 1) {
                const allFilled = Array.from(digits).every(d => d.value.length === 1);
                if (allFilled) {
                    setTimeout(() => dom.verifyBtn.click(), 100);
                }
            }
        });
        
        // Handle backspace
        digit.addEventListener('keydown', (e) => {
            if (e.key === 'Backspace' && !e.target.value && i > 0) {
                digits[i - 1].focus();
            }
        });
        
        // Only allow digits
        digit.addEventListener('keypress', (e) => {
            if (!/\d/.test(e.key) && !['Backspace', 'Delete', 'Tab'].includes(e.key)) {
                e.preventDefault();
            }
        });
    });
}

/**
 * Initialize authentication module
 */
export function initAuth(state, dom) {
    // Check for existing token
    const token = localStorage.getItem('cre_token');
    if (token) {
        state.token = token;
        dom.loginBtn.textContent = 'Connected';
        dom.loginBtn.classList.add('connected');
        dom.balancePill.classList.remove('hidden');
    }
    
    // Set up event listeners
    if (dom.sendCodeBtn) {
        dom.sendCodeBtn.addEventListener('click', () => sendCode(state, dom));
    }
    
    if (dom.verifyBtn) {
        dom.verifyBtn.addEventListener('click', () => verifyCode(state, dom));
    }
    
    if (dom.changeNumBtn) {
        dom.changeNumBtn.addEventListener('click', () => {
            dom.stepOtp.classList.remove('active');
            dom.stepPhone.classList.add('active');
            dom.phoneInput.focus();
        });
    }
    
    if (dom.loginBtn) {
        dom.loginBtn.addEventListener('click', () => {
            if (isAuthenticated(state)) {
                // Show account menu or logout option
                const menu = document.querySelector('.account-menu');
                if (menu) {
                    menu.classList.toggle('hidden');
                } else {
                    // Simple logout if no menu
                    if (confirm('Log out?')) {
                        logout(state, dom);
                    }
                }
            } else {
                showLogin(dom);
            }
        });
    }
    
    if (dom.modalClose) {
        dom.modalClose.addEventListener('click', () => hideLogin(dom));
    }
    
    // Click outside modal to close
    if (dom.modalBackdrop) {
        dom.modalBackdrop.addEventListener('click', (e) => {
            if (e.target === dom.modalBackdrop) {
                hideLogin(dom);
            }
        });
    }
    
    // Phone input formatting
    if (dom.phoneInput) {
        dom.phoneInput.addEventListener('input', (e) => {
            // Format as XXXX XXXX
            let value = e.target.value.replace(/\D/g, '');
            if (value.length > 4) {
                value = value.slice(0, 4) + ' ' + value.slice(4, 8);
            }
            e.target.value = value;
            
            // Enable/disable send button
            const cleanPhone = value.replace(/\D/g, '');
            dom.sendCodeBtn.disabled = cleanPhone.length !== 8;
        });
        
        // Send code on Enter
        dom.phoneInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter' && !dom.sendCodeBtn.disabled) {
                dom.sendCodeBtn.click();
            }
        });
    }
    
    // Setup OTP digit handlers
    setupOTPDigits(dom);
    
    // Verify code on Enter in OTP step
    document.addEventListener('keypress', (e) => {
        if (e.key === 'Enter' && dom.stepOtp.classList.contains('active')) {
            const digits = dom.stepOtp.querySelectorAll('.otp-digit');
            const otp = Array.from(digits).map(d => d.value).join('');
            if (otp.length === 6) {
                dom.verifyBtn.click();
            }
        }
    });
}