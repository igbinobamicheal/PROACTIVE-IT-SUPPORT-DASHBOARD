/**
 * BlurText - Vanilla JavaScript Component
 * Animates text characters or words with a blur-in effect when they enter the viewport.
 * Dependency-free, high performance.
 */
export default function initBlurText() {
    const targets = document.querySelectorAll('.blur-text-target');
    targets.forEach(target => {
        const text = target.textContent.trim();
        const animateBy = target.getAttribute('data-animate-by') || 'words';
        const direction = target.getAttribute('data-direction') || 'top';
        const delay = parseInt(target.getAttribute('data-delay') || '150', 10);
        const duration = target.getAttribute('data-duration') || '0.7s';
        
        // Split by words or letters
        const elements = animateBy === 'words' ? text.split(/\s+/) : text.split('');
        
        target.innerHTML = '';
        
        elements.forEach((segment, index) => {
            if (segment === '') return;
            
            const span = document.createElement('span');
            span.className = direction === 'top' ? 'blur-span-top' : 'blur-span-bottom';
            span.textContent = segment;
            span.style.animationDuration = duration;
            span.style.animationDelay = `${index * delay}ms`;
            
            // Handle animation complete callback on the last element if defined
            if (index === elements.length - 1) {
                span.addEventListener('animationend', () => {
                    const callbackName = target.getAttribute('data-on-complete');
                    if (callbackName && typeof window[callbackName] === 'function') {
                        window[callbackName]();
                    }
                });
            }
            
            target.appendChild(span);
            
            // Add space if animating by words, except after the last word
            if (animateBy === 'words' && index < elements.length - 1) {
                target.appendChild(document.createTextNode('\u00A0'));
            }
        });
        
        // Intersection Observer to trigger animation
        const observer = new IntersectionObserver(([entry]) => {
            if (entry.isIntersecting) {
                const spans = target.querySelectorAll('.blur-span-top, .blur-span-bottom');
                spans.forEach(span => {
                    span.classList.add('animate-active');
                });
                observer.unobserve(target);
            }
        }, {
            threshold: parseFloat(target.getAttribute('data-threshold') || '0.1'),
            rootMargin: target.getAttribute('data-root-margin') || '0px'
        });
        
        observer.observe(target);
    });
}
