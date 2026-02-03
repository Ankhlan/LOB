// CRE Auth Module
// Authentication and session management

const Auth = {
    // Current user
    user: null,
    
    // Session token
    token: null,
    
    // Session expiry
    expiresAt: null,
    
    // Initialize
    init() {
        this.loadSession();
        this.bindEvents();
    },
    
    // Load session from storage
    loadSession() {
        try {
            const stored = localStorage.getItem('cre_session');
            if (stored) {
                const data = JSON.parse(stored);
                
                // Check expiry
                if (data.expiresAt && new Date(data.expiresAt) > new Date()) {
                    this.token = data.token;
                    this.user = data.user;
                    this.expiresAt = new Date(data.expiresAt);
                    this.onLogin();
                } else {
                    // Expired
                    this.clearSession();
                }
            }
        } catch (e) {
            console.error('Failed to load session:', e);
            this.clearSession();
        }
    },
    
    // Save session to storage
    saveSession() {
        try {
            localStorage.setItem('cre_session', JSON.stringify({
                token: this.token,
                user: this.user,
                expiresAt: this.expiresAt ? this.expiresAt.toISOString() : null
            }));
        } catch (e) {
            console.error('Failed to save session:', e);
        }
    },
    
    // Clear session
    clearSession() {
        this.token = null;
        this.user = null;
        this.expiresAt = null;
        localStorage.removeItem('cre_session');
    },
    
    // Bind events
    bindEvents() {
        // Auto-logout on token expiry
        setInterval(() => {
            if (this.expiresAt && new Date() >= this.expiresAt) {
                this.logout('Session expired');
            }
        }, 60000); // Check every minute
    },
    
    // Check if logged in
    isLoggedIn() {
        return !!this.token && !!this.user;
    },
    
    // Get current user
    getUser() {
        return this.user;
    },
    
    // Get auth token
    getToken() {
        return this.token;
    },
    
    // Login
    async login(phone, password, countryCode = '+976') {
        try {
            const fullPhone = countryCode + phone.replace(/^0+/, '');
            
            const response = await API?.login(fullPhone, password);
            
            if (response && response.success && response.token) {
                this.token = response.token;
                this.user = response.user || { phone: fullPhone };
                this.expiresAt = response.expiresAt 
                    ? new Date(response.expiresAt) 
                    : new Date(Date.now() + 24 * 60 * 60 * 1000); // 24 hours default
                
                this.saveSession();
                this.onLogin();
                
                Toast?.success('Logged in successfully');
                return true;
            } else {
                Toast?.error(response?.message || 'Login failed');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Login failed');
            return false;
        }
    },
    
    // Logout
    logout(reason = null) {
        const wasLoggedIn = this.isLoggedIn();
        
        this.clearSession();
        this.onLogout();
        
        if (wasLoggedIn) {
            if (reason) {
                Toast?.info(reason);
            } else {
                Toast?.info('Logged out');
            }
        }
    },
    
    // Register
    async register(phone, password, otp, countryCode = '+976') {
        try {
            const fullPhone = countryCode + phone.replace(/^0+/, '');
            
            const response = await API?.register(fullPhone, password, otp);
            
            if (response && response.success) {
                Toast?.success('Registration successful. Please login.');
                return true;
            } else {
                Toast?.error(response?.message || 'Registration failed');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Registration failed');
            return false;
        }
    },
    
    // Request OTP
    async requestOTP(phone, countryCode = '+976') {
        try {
            const fullPhone = countryCode + phone.replace(/^0+/, '');
            
            const response = await API?.requestOTP(fullPhone);
            
            if (response && response.success) {
                Toast?.success('OTP sent to your phone');
                return true;
            } else {
                Toast?.error(response?.message || 'Failed to send OTP');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to send OTP');
            return false;
        }
    },
    
    // Reset password
    async resetPassword(phone, newPassword, otp, countryCode = '+976') {
        try {
            const fullPhone = countryCode + phone.replace(/^0+/, '');
            
            const response = await API?.resetPassword(fullPhone, newPassword, otp);
            
            if (response && response.success) {
                Toast?.success('Password reset successful');
                return true;
            } else {
                Toast?.error(response?.message || 'Failed to reset password');
                return false;
            }
        } catch (error) {
            Toast?.error(error.message || 'Failed to reset password');
            return false;
        }
    },
    
    // Called after successful login
    onLogin() {
        // Update UI
        const loginBtn = document.getElementById('loginBtn');
        const userInfo = document.getElementById('userInfo');
        const userPhone = document.getElementById('userPhone');
        
        if (loginBtn) loginBtn.style.display = 'none';
        if (userInfo) userInfo.style.display = 'flex';
        if (userPhone && this.user?.phone) {
            userPhone.textContent = this.user.phone;
        }
        
        // Update State module
        if (window.State) {
            State.set('user', this.user);
            State.set('isLoggedIn', true);
        }
        
        // Re-establish SSE if needed
        if (window.SSE && SSE.isConnected && SSE.isConnected()) {
            SSE.reconnect();
        }
    },
    
    // Called after logout
    onLogout() {
        // Update UI
        const loginBtn = document.getElementById('loginBtn');
        const userInfo = document.getElementById('userInfo');
        
        if (loginBtn) loginBtn.style.display = 'block';
        if (userInfo) userInfo.style.display = 'none';
        
        // Update State module
        if (window.State) {
            State.set('user', null);
            State.set('isLoggedIn', false);
        }
    },
    
    // Show login modal
    showLoginModal() {
        if (!window.Modal) {
            // Fallback to existing function
            if (window.showLogin) window.showLogin();
            return;
        }
        
        Modal.form({
            title: 'Login',
            fields: [
                { name: 'phone', label: 'Phone Number', type: 'tel', placeholder: '99001122', required: true },
                { name: 'password', label: 'Password', type: 'password', required: true }
            ],
            submitText: 'Login'
        }).then(data => {
            if (data) {
                this.login(data.phone, data.password);
            }
        });
    },
    
    // Show register modal
    showRegisterModal() {
        if (!window.Modal) {
            if (window.showRegister) window.showRegister();
            return;
        }
        
        Modal.form({
            title: 'Register',
            fields: [
                { name: 'phone', label: 'Phone Number', type: 'tel', placeholder: '99001122', required: true },
                { name: 'otp', label: 'OTP Code', type: 'text', placeholder: '6-digit code', required: true },
                { name: 'password', label: 'Password', type: 'password', required: true },
                { name: 'confirmPassword', label: 'Confirm Password', type: 'password', required: true }
            ],
            submitText: 'Register'
        }).then(data => {
            if (data) {
                if (data.password !== data.confirmPassword) {
                    Toast?.error('Passwords do not match');
                    return;
                }
                this.register(data.phone, data.password, data.otp);
            }
        });
    }
};

// Global functions for compatibility
window.logout = () => Auth.logout();
window.showLogin = () => Auth.showLoginModal();
window.showRegister = () => Auth.showRegisterModal();

// Export
if (typeof module !== 'undefined') {
    module.exports = Auth;
}

window.Auth = Auth;
// ES Module export
export { Auth };