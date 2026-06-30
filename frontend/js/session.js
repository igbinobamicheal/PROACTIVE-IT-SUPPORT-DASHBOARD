(function() {
    const INACTIVITY_TIMEOUT = 30 * 60 * 1000; // 30 minutes
    const WARNING_TIMEOUT = 29 * 60 * 1000;    // 29 minutes (1 minute warning)
    const CHECK_INTERVAL = 1000;               // Check every second

    // Update last activity timestamp
    function resetInactivityTimer() {
        if (!localStorage.getItem('jwt_token')) return;
        localStorage.setItem('session_last_activity', Date.now().toString());
        hideWarningModal();
    }

    // Set up activity listeners
    const activityEvents = ['mousemove', 'keydown', 'scroll', 'click', 'touchstart'];
    activityEvents.forEach(eventName => {
        window.addEventListener(eventName, throttle(resetInactivityTimer, 2000));
    });

    // Also reset timer on fetch API requests
    const originalFetch = window.fetch;
    window.fetch = function(...args) {
        resetInactivityTimer();
        return originalFetch.apply(this, args);
    };

    // Throttling function to prevent performance overhead on events like mousemove
    function throttle(func, limit) {
        let inThrottle;
        return function() {
            const args = arguments;
            const context = this;
            if (!inThrottle) {
                func.apply(context, args);
                inThrottle = true;
                setTimeout(() => inThrottle = false, limit);
            }
        }
    }

    // Modal creation and management
    let modal = null;
    let countdownInterval = null;

    function showWarningModal() {
        if (modal) return; // Already shown

        // Create the warning modal DOM element
        modal = document.createElement('div');
        modal.id = 'sessionWarningModal';
        modal.className = 'fixed inset-0 bg-black/40 backdrop-blur-sm z-[100] flex items-center justify-center';
        modal.innerHTML = `
            <div class="bg-surface/90 border border-white/50 backdrop-blur-2xl rounded-xl p-6 w-[400px] shadow-2xl glass-panel text-textMain relative flex flex-col gap-4">
                <div class="flex items-center gap-3 pb-2 border-b border-borderSubtle">
                    <div class="w-8 h-8 rounded-lg bg-amber-500/10 flex items-center justify-center text-primary">
                        <svg class="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"></circle><polyline points="12 6 12 12 16 14"></polyline></svg>
                    </div>
                    <div>
                        <h3 class="font-bold text-[14px]">Session Timing Out</h3>
                        <p class="text-[11px] text-textSubtle mt-0.5">Your session is about to expire due to inactivity.</p>
                    </div>
                </div>
                
                <div class="text-[13px] text-textMuted leading-relaxed">
                    You will be automatically logged out in <span id="sessionCountdown" class="font-mono font-bold text-primary">60</span> seconds. Would you like to extend your session?
                </div>

                <div class="flex gap-3 mt-2">
                    <button id="btnExtendSession" class="flex-1 bg-primary hover:bg-primaryHover text-white py-2 rounded-md font-semibold text-[12px] transition-all flex items-center justify-center gap-2 shadow-sm border border-transparent">
                        Extend Session
                    </button>
                    <button id="btnForceLogout" class="flex-1 border border-borderSubtle bg-surface hover:bg-black/[0.02] py-2 rounded-md font-semibold text-[12px] transition-all text-textMain">
                        Logout
                    </button>
                </div>
            </div>
        `;
        document.body.appendChild(modal);

        document.getElementById('btnExtendSession').addEventListener('click', () => {
            resetInactivityTimer();
        });

        document.getElementById('btnForceLogout').addEventListener('click', () => {
            logout();
        });

        // Initialize modal countdown
        let secondsLeft = 60;
        const countdownEl = document.getElementById('sessionCountdown');
        countdownInterval = setInterval(() => {
            // Check if another tab extended the session
            const elapsed = Date.now() - parseInt(localStorage.getItem('session_last_activity') || '0');
            if (elapsed < WARNING_TIMEOUT) {
                hideWarningModal();
                return;
            }

            secondsLeft = Math.max(0, Math.ceil((INACTIVITY_TIMEOUT - elapsed) / 1000));
            if (countdownEl) countdownEl.textContent = secondsLeft;

            if (secondsLeft <= 0) {
                logout();
            }
        }, 1000);
    }

    function hideWarningModal() {
        if (modal) {
            modal.remove();
            modal = null;
        }
        if (countdownInterval) {
            clearInterval(countdownInterval);
            countdownInterval = null;
        }
    }

    function logout() {
        hideWarningModal();
        localStorage.removeItem('jwt_token');
        localStorage.removeItem('username');
        localStorage.removeItem('session_last_activity');
        window.location.href = 'login.html';
    }

    // Check timer loop
    function checkSession() {
        const token = localStorage.getItem('jwt_token');
        if (!token) return;

        const lastActivityStr = localStorage.getItem('session_last_activity');
        if (!lastActivityStr) {
            resetInactivityTimer();
            return;
        }

        const elapsed = Date.now() - parseInt(lastActivityStr);
        if (elapsed >= INACTIVITY_TIMEOUT) {
            logout();
        } else if (elapsed >= WARNING_TIMEOUT) {
            showWarningModal();
        } else {
            hideWarningModal();
        }
    }

    // Start checking
    if (localStorage.getItem('jwt_token')) {
        // Initialize timer if not set
        if (!localStorage.getItem('session_last_activity')) {
            resetInactivityTimer();
        }
        setInterval(checkSession, CHECK_INTERVAL);
    }
})();
