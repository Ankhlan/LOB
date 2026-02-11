const puppeteer = require('puppeteer');
const path = require('path');

async function testModules() {
    console.log('🦊 Testing CRE.mn modular split...');
    
    const browser = await puppeteer.launch({ headless: false });
    const page = await browser.newPage();
    
    // Navigate to test page
    const testPath = 'file://' + path.resolve(__dirname, 'module-test.html');
    console.log('Loading:', testPath);
    
    await page.goto(testPath);
    
    // Wait for tests to complete
    await page.waitForTimeout(3000);
    
    // Get test results
    const results = await page.evaluate(() => {
        const testDivs = document.querySelectorAll('.test');
        const results = [];
        testDivs.forEach(div => {
            const passed = div.classList.contains('pass');
            const text = div.textContent;
            results.push({ passed, text });
        });
        return results;
    });
    
    // Output results
    console.log('\n=== MODULE TEST RESULTS ===');
    let passCount = 0;
    let totalCount = 0;
    
    results.forEach(result => {
        totalCount++;
        if (result.passed) {
            passCount++;
            console.log('✅', result.text);
        } else {
            console.log('❌', result.text);
        }
    });
    
    console.log(`\n📊 SUMMARY: ${passCount}/${totalCount} tests passed`);
    
    if (passCount === totalCount) {
        console.log('🎉 ALL TESTS PASSED! Modular split is working correctly.');
    } else {
        console.log('⚠️ Some tests failed. Check the issues above.');
    }
    
    await browser.close();
    return passCount === totalCount;
}

testModules().catch(console.error);