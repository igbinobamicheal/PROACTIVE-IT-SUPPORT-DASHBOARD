/**
 * ShapeGrid - Subtle animated background grid
 * Pure vanilla JS, zero dependencies, canvas-based for performance.
 * Automatically injects itself as a fixed full-screen background layer.
 *
 * Colors are derived from the site's warm amber/earth palette:
 *   - Background: #ECEAE5
 *   - Primary:    #B87C3B (Rich Amber)
 *   - Text:       #1E1A17
 *
 * The grid uses the primary amber at very low opacity (~6-10%) for lines
 * and subtle hover-glow effects, ensuring it blends seamlessly.
 */
(function () {
    'use strict';

    // --- Configuration ---
    var CELL_SIZE = 60;
    var LINE_COLOR = 'rgba(184, 124, 59, 0.055)';       // primary amber at ~5.5%
    var SHAPE_COLOR = 'rgba(184, 124, 59, 0.045)';       // shape fill at ~4.5%
    var SHAPE_STROKE = 'rgba(184, 124, 59, 0.08)';       // shape border at ~8%
    var GLOW_COLOR = 'rgba(184, 124, 59, 0.12)';         // hover proximity glow at ~12%
    var GLOW_RADIUS = 180;                                // px radius of cursor proximity glow
    var SHAPE_PROBABILITY = 0.12;                         // chance a cell contains a shape
    var ANIMATION_SPEED = 0.0004;                         // rotation/pulse speed (very slow)
    var LINE_WIDTH = 0.5;

    // --- State ---
    var canvas, ctx, width, height, cols, rows;
    var shapes = [];
    var mouseX = -9999, mouseY = -9999;
    var animFrame;
    var time = 0;
    var dpr = Math.min(window.devicePixelRatio || 1, 2);

    // --- Shape types ---
    var SHAPE_TYPES = ['circle', 'square', 'diamond', 'triangle', 'ring'];

    function init() {
        // Create and inject canvas
        canvas = document.createElement('canvas');
        canvas.id = 'shapegrid-bg';
        canvas.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;z-index:-1;pointer-events:none;';
        document.body.insertBefore(canvas, document.body.firstChild);

        ctx = canvas.getContext('2d');

        resize();
        generateShapes();

        // Passive mouse tracking for glow effect
        document.addEventListener('mousemove', onMouseMove, { passive: true });
        window.addEventListener('resize', onResize, { passive: true });

        // Start render loop
        tick();
    }

    function resize() {
        width = window.innerWidth;
        height = window.innerHeight;
        canvas.width = width * dpr;
        canvas.height = height * dpr;
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        cols = Math.ceil(width / CELL_SIZE) + 1;
        rows = Math.ceil(height / CELL_SIZE) + 1;
    }

    function generateShapes() {
        shapes = [];
        for (var r = 0; r < rows; r++) {
            for (var c = 0; c < cols; c++) {
                if (Math.random() < SHAPE_PROBABILITY) {
                    shapes.push({
                        x: c * CELL_SIZE + CELL_SIZE / 2,
                        y: r * CELL_SIZE + CELL_SIZE / 2,
                        type: SHAPE_TYPES[Math.floor(Math.random() * SHAPE_TYPES.length)],
                        size: 6 + Math.random() * 8,
                        phase: Math.random() * Math.PI * 2,
                        rotSpeed: (Math.random() - 0.5) * 0.3
                    });
                }
            }
        }
    }

    function onMouseMove(e) {
        mouseX = e.clientX;
        mouseY = e.clientY;
    }

    var resizeTimer;
    function onResize() {
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(function () {
            resize();
            generateShapes();
        }, 200);
    }

    function tick() {
        time += ANIMATION_SPEED;
        draw();
        animFrame = requestAnimationFrame(tick);
    }

    function draw() {
        ctx.clearRect(0, 0, width, height);

        // Draw grid lines
        ctx.strokeStyle = LINE_COLOR;
        ctx.lineWidth = LINE_WIDTH;
        ctx.beginPath();
        for (var c = 0; c <= cols; c++) {
            var x = c * CELL_SIZE;
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
        }
        for (var r = 0; r <= rows; r++) {
            var y = r * CELL_SIZE;
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
        }
        ctx.stroke();

        // Draw cursor proximity glow (soft radial gradient)
        if (mouseX > -999 && mouseY > -999) {
            var glow = ctx.createRadialGradient(mouseX, mouseY, 0, mouseX, mouseY, GLOW_RADIUS);
            glow.addColorStop(0, GLOW_COLOR);
            glow.addColorStop(1, 'rgba(184, 124, 59, 0)');
            ctx.fillStyle = glow;
            ctx.fillRect(mouseX - GLOW_RADIUS, mouseY - GLOW_RADIUS, GLOW_RADIUS * 2, GLOW_RADIUS * 2);
        }

        // Draw shapes
        for (var i = 0; i < shapes.length; i++) {
            var s = shapes[i];

            // Subtle breathing pulse
            var pulse = 1 + Math.sin(time * 800 + s.phase) * 0.15;
            var sz = s.size * pulse;

            // Proximity-based opacity boost
            var dx = s.x - mouseX;
            var dy = s.y - mouseY;
            var dist = Math.sqrt(dx * dx + dy * dy);
            var proximityAlpha = dist < GLOW_RADIUS ? (1 - dist / GLOW_RADIUS) * 0.12 : 0;

            ctx.save();
            ctx.translate(s.x, s.y);
            ctx.rotate(time * 200 * s.rotSpeed * ANIMATION_SPEED + s.phase);

            ctx.globalAlpha = 1;
            ctx.fillStyle = proximityAlpha > 0
                ? 'rgba(184, 124, 59, ' + (0.045 + proximityAlpha) + ')'
                : SHAPE_COLOR;
            ctx.strokeStyle = proximityAlpha > 0
                ? 'rgba(184, 124, 59, ' + (0.08 + proximityAlpha) + ')'
                : SHAPE_STROKE;
            ctx.lineWidth = 0.8;

            drawShape(s.type, sz);

            ctx.restore();
        }
    }

    function drawShape(type, sz) {
        switch (type) {
            case 'circle':
                ctx.beginPath();
                ctx.arc(0, 0, sz, 0, Math.PI * 2);
                ctx.fill();
                ctx.stroke();
                break;
            case 'ring':
                ctx.beginPath();
                ctx.arc(0, 0, sz, 0, Math.PI * 2);
                ctx.stroke();
                break;
            case 'square':
                ctx.beginPath();
                ctx.rect(-sz, -sz, sz * 2, sz * 2);
                ctx.fill();
                ctx.stroke();
                break;
            case 'diamond':
                ctx.beginPath();
                ctx.moveTo(0, -sz);
                ctx.lineTo(sz, 0);
                ctx.lineTo(0, sz);
                ctx.lineTo(-sz, 0);
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
                break;
            case 'triangle':
                ctx.beginPath();
                ctx.moveTo(0, -sz);
                ctx.lineTo(sz * 0.866, sz * 0.5);
                ctx.lineTo(-sz * 0.866, sz * 0.5);
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
                break;
        }
    }

    // --- Boot ---
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
