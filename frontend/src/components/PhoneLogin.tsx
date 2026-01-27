import { useState } from 'react';
import { api } from '../api';
import './PhoneLogin.css';

interface PhoneLoginProps {
  onLogin: () => void;
}

export function PhoneLogin({ onLogin }: PhoneLoginProps) {
  const [phone, setPhone] = useState('+976');
  const [otp, setOtp] = useState('');
  const [step, setStep] = useState<'phone' | 'otp'>('phone');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [devOtp, setDevOtp] = useState<string | null>(null);

  const handleRequestOtp = async () => {
    setError(null);
    setLoading(true);
    
    try {
      const res = await api.requestOtp(phone);
      if (res.success) {
        setStep('otp');
        if (res.dev_otp) {
          setDevOtp(res.dev_otp);
        }
      } else {
        setError(res.error || 'Failed to send OTP');
      }
    } catch (e) {
      setError('Network error. Please try again.');
    } finally {
      setLoading(false);
    }
  };

  const handleVerifyOtp = async () => {
    setError(null);
    setLoading(true);
    
    try {
      const res = await api.verifyOtp(phone, otp);
      if (res.success) {
        onLogin();
      } else {
        setError(res.error || 'Invalid OTP');
      }
    } catch (e) {
      setError('Network error. Please try again.');
    } finally {
      setLoading(false);
    }
  };

  const formatPhone = (value: string) => {
    // Keep +976 prefix and allow 8 more digits
    if (!value.startsWith('+976')) {
      return '+976';
    }
    const digits = value.slice(4).replace(/\D/g, '').slice(0, 8);
    return '+976' + digits;
  };

  return (
    <div className="phone-login">
      <div className="login-card">
        <div className="login-header">
          <span className="flag">🇲🇳</span>
          <h1>Central Exchange</h1>
          <p>Mongolia's First Digital Derivatives Exchange</p>
        </div>

        {step === 'phone' ? (
          <div className="login-form">
            <label>Phone Number</label>
            <input
              type="tel"
              value={phone}
              onChange={(e) => setPhone(formatPhone(e.target.value))}
              placeholder="+976 9900 1234"
              disabled={loading}
            />
            <p className="hint">Enter your Mongolian mobile number</p>
            
            {error && <div className="error">{error}</div>}
            
            <button 
              onClick={handleRequestOtp}
              disabled={loading || phone.length < 12}
              className="primary-btn"
            >
              {loading ? 'Sending...' : 'Get OTP'}
            </button>
          </div>
        ) : (
          <div className="login-form">
            <label>Enter OTP</label>
            <input
              type="text"
              value={otp}
              onChange={(e) => setOtp(e.target.value.replace(/\D/g, '').slice(0, 6))}
              placeholder="6-digit code"
              maxLength={6}
              disabled={loading}
              autoFocus
            />
            <p className="hint">Sent to {phone}</p>
            
            {devOtp && (
              <div className="dev-otp">
                DEV MODE: OTP is <strong>{devOtp}</strong>
              </div>
            )}
            
            {error && <div className="error">{error}</div>}
            
            <button 
              onClick={handleVerifyOtp}
              disabled={loading || otp.length !== 6}
              className="primary-btn"
            >
              {loading ? 'Verifying...' : 'Login'}
            </button>
            
            <button 
              onClick={() => { setStep('phone'); setOtp(''); setError(null); }}
              className="secondary-btn"
              disabled={loading}
            >
              Change Number
            </button>
          </div>
        )}

        <div className="login-footer">
          <p>MNT-Quoted Products | FXCM Backed | C++ Engine</p>
        </div>
      </div>
    </div>
  );
}
