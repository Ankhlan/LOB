import { useState, useEffect } from 'react';
import { api } from './api';
import { PhoneLogin } from './components/PhoneLogin';
import { Dashboard } from './components/Dashboard';
import './App.css';

function App() {
  const [isLoggedIn, setIsLoggedIn] = useState(false);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    // Check if user has a valid token
    const checkAuth = async () => {
      if (api.getToken()) {
        const user = await api.getMe();
        if (user) {
          setIsLoggedIn(true);
        }
      }
      setLoading(false);
    };
    
    checkAuth();
  }, []);

  if (loading) {
    return (
      <div className="loading-screen">
        <div className="loader">
          <span className="flag">🇲🇳</span>
          <span>Loading...</span>
        </div>
      </div>
    );
  }

  if (!isLoggedIn) {
    return <PhoneLogin onLogin={() => setIsLoggedIn(true)} />;
  }

  return <Dashboard onLogout={() => setIsLoggedIn(false)} />;
}

export default App;
