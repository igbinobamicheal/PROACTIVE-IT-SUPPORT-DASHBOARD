// Deploy Agent Page Logic
let activeRegToken = '';
let countdownTimer = null;
let pollTimer = null;

document.addEventListener('DOMContentLoaded', () => {
    const generateTokenBtn = document.getElementById('generateTokenBtn');
    const copyBtn = document.getElementById('copyBtn');
    const downloadBtn = document.getElementById('downloadBtn');

    if (generateTokenBtn) {
        generateTokenBtn.addEventListener('click', () => {
            generateDeploymentToken();
        });
    }

    if (copyBtn) {
        copyBtn.addEventListener('click', () => {
            copyDeploymentToken();
        });
    }

    if (downloadBtn) {
        downloadBtn.addEventListener('click', () => {
            downloadAgentInstaller();
        });
    }
});

async function generateDeploymentToken() {
    const step1Pane = document.getElementById('step1Pane');
    const step2Pane = document.getElementById('step2Pane');
    const tokenText = document.getElementById('tokenText');
    const statusDot = document.getElementById('statusDot');
    const statusLabel = document.getElementById('statusLabel');

    try {
        const res = await api.createRegistrationToken();
        activeRegToken = res.token;

        // Transition panes
        step1Pane.classList.add('hidden');
        step2Pane.classList.remove('hidden');

        // Set token values
        tokenText.textContent = activeRegToken;
        window.lucide.createIcons();

        // Update status indicators
        statusDot.className = 'w-1.5 h-1.5 rounded-full bg-primary animate-ping';
        statusLabel.textContent = 'Waiting for agent to report...';

        // Start 15 minutes timer
        startCountdown(15 * 60);

        // Start polling loop
        startRegistrationPolling(activeRegToken);

    } catch (err) {
        alert('Failed to generate registration token: ' + err.message);
    }
}

function startCountdown(durationSeconds) {
    if (countdownTimer) clearInterval(countdownTimer);

    let remaining = durationSeconds;
    const countdownEl = document.getElementById('countdownText');

    const update = () => {
        const mins = Math.floor(remaining / 60);
        const secs = remaining % 60;
        countdownEl.textContent = `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;

        if (remaining <= 0) {
            clearInterval(countdownTimer);
            if (pollTimer) clearInterval(pollTimer);
            
            countdownEl.textContent = 'EXPIRED';
            
            const statusDot = document.getElementById('statusDot');
            const statusLabel = document.getElementById('statusLabel');
            statusDot.className = 'w-1.5 h-1.5 rounded-full bg-danger';
            statusLabel.textContent = 'Token expired';

            alert('The registration token has expired. Please refresh the page and generate a new one.');
            location.reload();
        }
        remaining--;
    };

    update();
    countdownTimer = setInterval(update, 1000);
}

function startRegistrationPolling(tokenStr) {
    if (pollTimer) clearInterval(pollTimer);

    const step2Pane = document.getElementById('step2Pane');
    const step3Pane = document.getElementById('step3Pane');
    const statusDot = document.getElementById('statusDot');
    const statusLabel = document.getElementById('statusLabel');

    pollTimer = setInterval(async () => {
        try {
            const tokens = await api.getRegistrationTokens();
            const matchingToken = tokens.find(t => t.token === tokenStr);

            if (matchingToken && matchingToken.used) {
                // Succeeded! Clear timers
                clearInterval(pollTimer);
                if (countdownTimer) clearInterval(countdownTimer);

                // Transition UI
                step2Pane.classList.add('hidden');
                step3Pane.classList.remove('hidden');
                window.lucide.createIcons();

                // Update status checklist
                statusDot.className = 'w-1.5 h-1.5 rounded-full bg-success shadow-[0_0_8px_rgba(16,185,129,0.3)]';
                statusLabel.textContent = 'Onboarding complete!';
            }
        } catch (e) {
            console.error('Failed to poll token status:', e);
        }
    }, 3000);
}

function copyDeploymentToken() {
    if (!activeRegToken) return;

    navigator.clipboard.writeText(activeRegToken).then(() => {
        const copyIcon = document.getElementById('copyIcon');
        const copyText = document.getElementById('copyText');
        const copyBtn = document.getElementById('copyBtn');

        copyText.textContent = 'Copied!';
        copyBtn.classList.replace('bg-surface', 'bg-success/15');
        copyBtn.classList.add('text-success');

        setTimeout(() => {
            copyText.textContent = 'Copy';
            copyBtn.classList.replace('bg-success/15', 'bg-surface');
            copyBtn.classList.remove('text-success');
        }, 2000);
    });
}

function downloadAgentInstaller() {
    const base = window.API_BASE_URL || 'http://localhost:8080/api';
    window.location.href = `${base}/deploy/installer`;
}
