/**
 * Module Verification Test
 * Checks that all ES6 modules can be imported without errors
 */

// Test imports
import { state, dom, initState } from './modules/state.js';
import { connectSSE, registerEventHandler } from './modules/sse.js';
import { Sound } from './modules/sound.js';
import { fmt, fmtPct, toast } from './modules/utils.js';
import { onMarketsMsg, selectMarket } from './modules/market.js';
import { onBookMsg, fetchBook } from './modules/book.js';
import { loadChart } from './modules/chart.js';
import { sendCode, verifyCode, isAuthenticated } from './modules/auth.js';

console.log('✅ All modules imported successfully!');

// Test basic functionality
console.log('Testing module functions...');

// Test state
console.log('State keys:', Object.keys(state));

// Test utils
console.log('fmt(1234.56, 2):', fmt(1234.56, 2));
console.log('fmtPct(0.0525):', fmtPct(0.0525));

// Test sound
console.log('Sound methods:', Object.keys(Sound));

console.log('✅ Basic module tests passed!');

window.ModuleTest = {
    state,
    dom,
    Sound,
    fmt,
    fmtPct,
    selectMarket,
    loadChart,
    isAuthenticated
};