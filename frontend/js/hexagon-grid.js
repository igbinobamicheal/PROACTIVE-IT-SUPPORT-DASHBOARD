/**
 * Hexagon Grid Background Animation (Light Theme, Hero Section Only)
 * Renders a mathematical, interactive, pulsing copper hexagon grid on an HTML5 canvas.
 * Restrained to the top 650px (hero section) with page-relative mouse coordinates.
 */
(function() {
    function initHexagonGrid() {
        const canvas = document.getElementById('hexagon-grid-canvas');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        let width = canvas.width = window.innerWidth;
        let height = canvas.height = 650; // Constrained to hero section height

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
                // Higher opacity for visibility on light cream background (#ECEAE5)
                this.baseOpacity = 0.20 + Math.random() * 0.10;
                this.opacity = this.baseOpacity;
                this.pulseSpeed = 0.003 + Math.random() * 0.007;
                this.pulseDir = Math.random() > 0.5 ? 1 : -1;
                this.glow = 0;
            }

            update(mouseX, mouseY) {
                // Animate idle pulse
                this.opacity += this.pulseSpeed * this.pulseDir;
                if (this.opacity > 0.35 || this.opacity < 0.15) {
                    this.pulseDir *= -1;
                }

                // Interactive mouse/touch proximity glow
                if (mouseX !== undefined && mouseY !== undefined) {
                    const dx = this.x - mouseX;
                    const dy = this.y - mouseY;
                    const dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist < 220) {
                        const factor = (220 - dist) / 220;
                        this.glow = factor * factor * 0.65; // Quadratic falloff
                    } else {
                        this.glow += (0 - this.glow) * 0.08;
                    }
                } else {
                    this.glow += (0 - this.glow) * 0.08;
                }
            }

            draw(ctx) {
                const alpha = this.opacity + this.glow;
                // Use copper rgb(184, 124, 59) matching light theme primary
                ctx.strokeStyle = `rgba(184, 124, 59, ${alpha})`;
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

                // Draw soft fill on hover
                if (this.glow > 0.05) {
                    ctx.fillStyle = `rgba(184, 124, 59, ${this.glow * 0.05})`;
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

                    // Offset columns to lock hexagons together
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
                height = canvas.height = 650;
                createGrid();
            }, 150);
        });

        // Document-relative mouse listeners (resolves scroll offset)
        let mouseX, mouseY;
        window.addEventListener('mousemove', (e) => {
            if (e.pageY > 650) {
                mouseX = undefined;
                mouseY = undefined;
            } else {
                mouseX = e.pageX;
                mouseY = e.pageY;
            }
        });

        window.addEventListener('mouseleave', () => {
            mouseX = undefined;
            mouseY = undefined;
        });

        // Touch support for mobile layouts
        window.addEventListener('touchmove', (e) => {
            if (e.touches.length > 0) {
                const touchY = e.touches[0].pageY;
                if (touchY > 650) {
                    mouseX = undefined;
                    mouseY = undefined;
                } else {
                    mouseX = e.touches[0].pageX;
                    mouseY = touchY;
                }
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
