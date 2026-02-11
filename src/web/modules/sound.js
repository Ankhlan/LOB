/**
 * CRE.mn Trading Platform — Sound Module
 * =====================================
 * Web Audio API sound effects system
 */

/**
 * Sound system with Web Audio API
 * No external files - all sounds generated programmatically
 */
export const Sound = (() => {
    let ctx;
    
    function getCtx() {
        if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
        return ctx;
    }
    
    function beep(freq, dur, type = 'sine', vol = 0.15) {
        if (localStorage.getItem('cre_sound') === 'off') return;
        try {
            const c = getCtx();
            const o = c.createOscillator();
            const g = c.createGain();
            o.type = type;
            o.frequency.value = freq;
            g.gain.value = vol;
            g.gain.exponentialRampToValueAtTime(0.001, c.currentTime + dur);
            o.connect(g);
            g.connect(c.destination);
            o.start(c.currentTime);
            o.stop(c.currentTime + dur);
        } catch(e) {
            // Silently fail if Web Audio API not available
        }
    }
    
    return {
        orderFilled: () => { 
            beep(880, 0.1); 
            setTimeout(() => beep(1100, 0.15), 120); 
        },
        orderRejected: () => { 
            beep(300, 0.2, 'square', 0.1); 
        },
        orderPlaced: () => beep(660, 0.08),
        connected: () => { 
            beep(440, 0.08); 
            setTimeout(() => beep(660, 0.08), 100); 
            setTimeout(() => beep(880, 0.1), 200); 
        },
    };
})();