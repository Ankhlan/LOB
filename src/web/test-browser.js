/**
 * Browser Test for Modular App
 */

// Create a simple test HTML that loads our modular app
const testHtml = `
<!DOCTYPE html>
<html>
<head>
    <title>Module Test</title>
    <style>
        body { font-family: Arial; padding: 20px; }
        .test { margin: 10px 0; padding: 10px; border-left: 3px solid #ccc; }
        .pass { border-color: green; background: #f0f8ff; }
        .fail { border-color: red; background: #ffe4e1; }
    </style>
</head>
<body>
    <h1>CRE.mn Modular App Test</h1>
    <div id="results"></div>
    
    <script type="module">
        const results = document.getElementById('results');
        
        function addResult(test, status, detail = '') {
            const div = document.createElement('div');
            div.className = 'test ' + (status ? 'pass' : 'fail');
            div.innerHTML = '<strong>' + test + '</strong>: ' + (status ? '✅ PASS' : '❌ FAIL') + 
                           (detail ? '<br><small>' + detail + '</small>' : '');
            results.appendChild(div);
        }
        
        try {
            // Test 1: Import utils module
            const { fmt, fmtPct, toast } = await import('./modules/utils.js');
            addResult('Utils Module Import', true, 'Functions: fmt, fmtPct, toast');
            
            // Test 2: Test utility functions
            const formatted = fmt(1234.56, 2);
            addResult('fmt() function', formatted === '1,234.56', 'Result: ' + formatted);
            
            const percent = fmtPct(0.0525);
            addResult('fmtPct() function', percent === '+5.25%', 'Result: ' + percent);
            
            // Test 3: Import state module
            const { state, dom } = await import('./modules/state.js');
            addResult('State Module Import', true, 'State and DOM objects loaded');
            
            // Test 4: Import SSE module
            const { connectSSE, registerEventHandler } = await import('./modules/sse.js');
            addResult('SSE Module Import', true, 'connectSSE and registerEventHandler available');
            
            // Test 5: Import Sound module
            const { Sound } = await import('./modules/sound.js');
            addResult('Sound Module Import', true, 'Sound object with methods: ' + Object.keys(Sound).join(', '));
            
            // Test 6: Import Market module
            const { onMarketsMsg, selectMarket } = await import('./modules/market.js');
            addResult('Market Module Import', true, 'Market functions loaded');
            
            // Test 7: Import Book module
            const { onBookMsg, fetchBook } = await import('./modules/book.js');
            addResult('Book Module Import', true, 'Order book functions loaded');
            
            // Test 8: Import Chart module
            const { loadChart } = await import('./modules/chart.js');
            addResult('Chart Module Import', true, 'Chart functions loaded');
            
            // Test 9: Import Auth module
            const { sendCode, verifyCode, isAuthenticated } = await import('./modules/auth.js');
            addResult('Auth Module Import', true, 'Authentication functions loaded');
            
            addResult('OVERALL RESULT', true, 'All 9 modules imported successfully! 🎉');
            
        } catch (error) {
            addResult('Module Test', false, error.message);
        }
    </script>
</body>
</html>
`;

window.testHtml = testHtml;
console.log('Test HTML prepared for browser testing');