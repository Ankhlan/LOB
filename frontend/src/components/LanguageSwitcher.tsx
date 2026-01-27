import { useState } from 'react';
import { getLanguage, setLanguage, type Language } from '../i18n';
import './LanguageSwitcher.css';

interface LanguageSwitcherProps {
  onChange?: () => void;
}

export function LanguageSwitcher({ onChange }: LanguageSwitcherProps) {
  const [lang, setLang] = useState<Language>(getLanguage());

  const handleSwitch = () => {
    const newLang = lang === 'en' ? 'mn' : 'en';
    setLanguage(newLang);
    setLang(newLang);
    onChange?.();
    // Force re-render of entire app
    window.location.reload();
  };

  return (
    <button className="lang-switcher" onClick={handleSwitch}>
      <span className={lang === 'en' ? 'active' : ''}>EN</span>
      <span className="divider">/</span>
      <span className={lang === 'mn' ? 'active' : ''}>MN</span>
    </button>
  );
}
