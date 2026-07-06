/**
 * Hexagon Grid Background Animation
 * Renders a mathematical, interactive, pulsing copper hexagon grid on an HTML5 canvas.
 * Fully compatible with file:// protocol and classic script loading.
 */
(function() {
    function initHexagonGrid() {
        const canvas = document.getElementById('hexagon-grid-canvas');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        let width = canvas.width = window.innerWidth;
        let height = canvas.height = window.innerHeight;

        // Grid spacing constants based on hexagon geometry
        const hexRadius = 45; 
        const hexHeight = hexRadius * Math.sqrt(3);
        const horizDist = hexRadius * 1.5;
        const vertDist = hexHeight;

        const hexagons = [];

        class Hexagon {
            constructor(x, y, row, col) {
                this.x = x;
                this.y = y;
                this.row = row;
                this.col = col;
                // Subtle organic base pulsing opacity
                this.baseOpacity = 0.04 + Math.random() * 0.06;
                this.opacity = this.baseOpacity;
                this.pulseSpeed = 0.003 + Math.random() * 0.007;
                this.pulseDir = Math.random() > 0.5 ? 1 : -1;
                this.glow = 0;
            }

            update(mouseX, mouseY) {
                // Animate idle pulse
                this.opacity += this.pulseSpeed * this.pulseDir;
                if (this.opacity > 0.15 || this.opacity < 0.03) {
                    this.pulseDir *= -1;
                }

                // Interactive mouse/touch proximity glow
                if (mouseX !== undefined && mouseY !== undefined) {
                    const dx = this.x - mouseX;
                    const dy = this.y - mouseY;
                    const dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist < 220) {
                        const factor = (220 - dist) / 220;
                        this.glow = factor * factor * 0.5; // Quadratic falloff for smooth fading
                    } else {
                        this.glow += (0 - this.glow) * 0.08;
                    }
                } else {
                    this.glow += (0 - this.glow) * 0.08;
                }
            }

            draw(ctx) {
                const alpha = this.opacity + this.glow;
                ctx.strokeStyle = `rgba(217, 140, 69, ${alpha})`;
                ctx.lineWidth = 1 + this.glow * 1.5;

                ctx.beginPath();
                for (let i = 0; i < 6; i++) {
                    const angle = (Math.PI / 3) * i;
                    const px = this.x + hexRadius * Math.cos(angle);
                    const py = this.y + hexRadius * Math.sin(angle);
                    if (i === 0) {
                        ctx.moveTo(px, py);
                    } else {
                        ctx.lineTo(px, py);
                    }
                }
                ctx.closePath();
                ctx.stroke();

                // Draw soft fill on hovering nodes
                if (this.glow > 0.05) {
                    ctx.fillStyle = `rgba(217, 140, 69, ${this.glow * 0.04})`;
                    ctx.fill();
                }
            }
        }

        function createGrid() {
            hexagons.length = 0;
            const cols = Math.ceil(width / horizDist) + 2;
            const rows = Math.ceil(height / vertDist) + 2;

            for (let r = -1; r < rows; r++) {
                for (let c = -1; c < cols; c++) {
                    let x = c * horizDist;
                    let y = r * vertDist;

                    // Offset every other column to interlock hexagons
                    if (c % 2 !== 0) {
                        y += vertDist / 2;
                    }

                    hexagons.push(new Hexagon(x, y, r, c));
                }
            }
        }

        createGrid();

        // Handle resize debouncing
        let resizeTimeout;
        window.addEventListener('resize', () => {
            clearTimeout(resizeTimeout);
            resizeTimeout = setTimeout(() => {
                width = canvas.width = window.innerWidth;
                height = canvas.height = window.innerHeight;
                createGrid();
            }, 150);
        });

        // Mouse listeners
        let mouseX, mouseY;
        window.addEventListener('mousemove', (e) => {
            mouseX = e.clientX;
            mouseY = e.clientY;
        });

        window.addEventListener('mouseleave', () => {
            mouseX = undefined;
            mouseY = undefined;
        });

        // Touch support for mobile layouts
        window.addEventListener('touchmove', (e) => {
            if (e.touches.length > 0) {
                mouseX = e.touches[0].clientX;
                mouseY = e.touches[0].clientY;
            }
        });

        window.addEventListener('touchend', () => {
            mouseX = undefined;
            mouseY = undefined;
        });

        function animate() {
            ctx.clearRect(0, 0, width, height);

            hexagons.forEach(hex => {
                hex.update(mouseX, mouseY);
                hex.draw(ctx);
            });

            requestAnimationFrame(animate);
        }

        animate();
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initHexagonGrid);
    } else {
        initHexagonGrid();
    }
})();
