/**
 * WebGL Chart Module - Central Exchange
 * Using D3FC for high-performance candlestick rendering
 * KITSUNE 2026-01-30
 */

// D3FC WebGL Candlestick Renderer
class WebGLChart {
    constructor(container, options = {}) {
        this.container = typeof container === 'string'
            ? document.querySelector(container)
            : container;
        this.options = {
            upColor: options.upColor || [0.596, 0.765, 0.478, 1], // #98c379
            downColor: options.downColor || [0.878, 0.424, 0.459, 1], // #e06c75
            bgColor: options.bgColor || '#282c34',
            gridColor: options.gridColor || '#3e4451',
            textColor: options.textColor || '#abb2bf',
            ...options
        };
        this.data = [];
        this.xScale = null;
        this.yScale = null;
        this.chart = null;
        this.init();
    }

    init() {
        // Create canvas for WebGL
        this.canvas = document.createElement('canvas');
        this.canvas.style.width = '100%';
        this.canvas.style.height = '100%';
        this.container.innerHTML = '';
        this.container.appendChild(this.canvas);

        // Get WebGL context
        this.gl = this.canvas.getContext('webgl2') || this.canvas.getContext('webgl');
        if (!this.gl) {
            console.error('WebGL not supported');
            return;
        }

        // Resize handling
        this.resize();
        window.addEventListener('resize', () => this.resize());
    }

    resize() {
        const rect = this.container.getBoundingClientRect();
        const dpr = window.devicePixelRatio || 1;
        this.width = rect.width;
        this.height = rect.height;
        this.canvas.width = this.width * dpr;
        this.canvas.height = this.height * dpr;
        this.canvas.style.width = this.width + 'px';
        this.canvas.style.height = this.height + 'px';
        this.render();
    }

    setData(data) {
        this.data = data || [];
        this.updateScales();
        this.render();
    }

    updateScales() {
        if (!this.data || this.data.length === 0) return;

        const padding = { top: 20, right: 80, bottom: 30, left: 10 };
        const chartWidth = this.width - padding.left - padding.right;
        const chartHeight = this.height - padding.top - padding.bottom;

        // Time scale
        const timeExtent = [
            Math.min(...this.data.map(d => d.time)),
            Math.max(...this.data.map(d => d.time))
        ];

        // Price scale
        const priceMin = Math.min(...this.data.map(d => d.low));
        const priceMax = Math.max(...this.data.map(d => d.high));
        const pricePadding = (priceMax - priceMin) * 0.1;

        this.xScale = {
            domain: timeExtent,
            range: [padding.left, this.width - padding.right]
        };
        this.yScale = {
            domain: [priceMin - pricePadding, priceMax + pricePadding],
            range: [this.height - padding.bottom, padding.top]
        };
    }

    scaleX(value) {
        const { domain, range } = this.xScale;
        return range[0] + (value - domain[0]) / (domain[1] - domain[0]) * (range[1] - range[0]);
    }

    scaleY(value) {
        const { domain, range } = this.yScale;
        return range[0] + (value - domain[0]) / (domain[1] - domain[0]) * (range[1] - range[0]);
    }

    render() {
        const gl = this.gl;
        if (!gl || !this.data || this.data.length === 0) {
            this.renderEmpty();
            return;
        }

        const dpr = window.devicePixelRatio || 1;
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);

        // Clear with background color
        const bg = this.hexToRgb(this.options.bgColor);
        gl.clearColor(bg.r / 255, bg.g / 255, bg.b / 255, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);

        // Build vertex data for candlesticks
        const vertices = [];
        const colors = [];

        const candleCount = Math.min(this.data.length, 200);
        const startIdx = Math.max(0, this.data.length - candleCount);
        const candleWidth = (this.xScale.range[1] - this.xScale.range[0]) / candleCount * 0.8;

        for (let i = 0; i < candleCount; i++) {
            const candle = this.data[startIdx + i];
            const x = this.scaleX(candle.time);
            const yOpen = this.scaleY(candle.open);
            const yClose = this.scaleY(candle.close);
            const yHigh = this.scaleY(candle.high);
            const yLow = this.scaleY(candle.low);

            const isUp = candle.close >= candle.open;
            const color = isUp ? this.options.upColor : this.options.downColor;

            // Convert to normalized device coordinates
            const toNDC = (px, py) => [
                (px / this.width) * 2 - 1,
                -((py / this.height) * 2 - 1)
            ];

            // Wick (thin line)
            const wickWidth = 1;
            const [wxl, wyl] = toNDC(x - wickWidth / 2, yLow);
            const [wxr, wyh] = toNDC(x + wickWidth / 2, yHigh);

            // Two triangles for wick
            vertices.push(wxl, wyl, wxr, wyl, wxl, wyh);
            vertices.push(wxr, wyl, wxr, wyh, wxl, wyh);
            for (let j = 0; j < 6; j++) colors.push(...color);

            // Body (thick rectangle)
            const bodyTop = Math.min(yOpen, yClose);
            const bodyBottom = Math.max(yOpen, yClose);
            const [bxl, byl] = toNDC(x - candleWidth / 2, bodyBottom);
            const [bxr, byt] = toNDC(x + candleWidth / 2, bodyTop);

            // Two triangles for body
            vertices.push(bxl, byl, bxr, byl, bxl, byt);
            vertices.push(bxr, byl, bxr, byt, bxl, byt);
            for (let j = 0; j < 6; j++) colors.push(...color);
        }

        // Create shaders
        const vertexShaderSource = `
            attribute vec2 a_position;
            attribute vec4 a_color;
            varying vec4 v_color;
            void main() {
                gl_Position = vec4(a_position, 0, 1);
                v_color = a_color;
            }
        `;

        const fragmentShaderSource = `
            precision mediump float;
            varying vec4 v_color;
            void main() {
                gl_FragColor = v_color;
            }
        `;

        const program = this.createProgram(gl, vertexShaderSource, fragmentShaderSource);
        gl.useProgram(program);

        // Position buffer
        const positionBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
        const positionLoc = gl.getAttribLocation(program, 'a_position');
        gl.enableVertexAttribArray(positionLoc);
        gl.vertexAttribPointer(positionLoc, 2, gl.FLOAT, false, 0, 0);

        // Color buffer
        const colorBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.STATIC_DRAW);
        const colorLoc = gl.getAttribLocation(program, 'a_color');
        gl.enableVertexAttribArray(colorLoc);
        gl.vertexAttribPointer(colorLoc, 4, gl.FLOAT, false, 0, 0);

        // Draw
        gl.drawArrays(gl.TRIANGLES, 0, vertices.length / 2);

        // Cleanup
        gl.deleteBuffer(positionBuffer);
        gl.deleteBuffer(colorBuffer);
        gl.deleteProgram(program);

        // Draw axes and labels (2D overlay)
        this.renderOverlay();
    }

    renderEmpty() {
        const gl = this.gl;
        if (!gl) return;
        const bg = this.hexToRgb(this.options.bgColor);
        gl.clearColor(bg.r / 255, bg.g / 255, bg.b / 255, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);
    }

    renderOverlay() {
        // Create 2D canvas overlay for text
        if (!this.overlayCanvas) {
            this.overlayCanvas = document.createElement('canvas');
            this.overlayCanvas.style.position = 'absolute';
            this.overlayCanvas.style.top = '0';
            this.overlayCanvas.style.left = '0';
            this.overlayCanvas.style.pointerEvents = 'none';
            this.container.style.position = 'relative';
            this.container.appendChild(this.overlayCanvas);
        }

        const canvas = this.overlayCanvas;
        const ctx = canvas.getContext('2d');
        const dpr = window.devicePixelRatio || 1;

        canvas.width = this.width * dpr;
        canvas.height = this.height * dpr;
        canvas.style.width = this.width + 'px';
        canvas.style.height = this.height + 'px';
        ctx.scale(dpr, dpr);

        // Draw price labels
        ctx.fillStyle = this.options.textColor;
        ctx.font = '11px Consolas, monospace';
        ctx.textAlign = 'left';

        if (this.yScale) {
            const { domain, range } = this.yScale;
            const steps = 5;
            for (let i = 0; i <= steps; i++) {
                const price = domain[0] + (domain[1] - domain[0]) * i / steps;
                const y = range[0] + (range[1] - range[0]) * i / steps;
                ctx.fillText(this.formatPrice(price), this.width - 75, y + 4);
            }
        }
    }

    createProgram(gl, vsSource, fsSource) {
        const vs = gl.createShader(gl.VERTEX_SHADER);
        gl.shaderSource(vs, vsSource);
        gl.compileShader(vs);

        const fs = gl.createShader(gl.FRAGMENT_SHADER);
        gl.shaderSource(fs, fsSource);
        gl.compileShader(fs);

        const program = gl.createProgram();
        gl.attachShader(program, vs);
        gl.attachShader(program, fs);
        gl.linkProgram(program);

        gl.deleteShader(vs);
        gl.deleteShader(fs);

        return program;
    }

    hexToRgb(hex) {
        const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
        return result ? {
            r: parseInt(result[1], 16),
            g: parseInt(result[2], 16),
            b: parseInt(result[3], 16)
        } : { r: 0, g: 0, b: 0 };
    }

    formatPrice(price) {
        if (price >= 1000000) return (price / 1000000).toFixed(2) + 'M';
        if (price >= 1000) return (price / 1000).toFixed(1) + 'K';
        return price.toFixed(2);
    }
}

// Export for use
if (typeof window !== 'undefined') {
    window.WebGLChart = WebGLChart;
}
