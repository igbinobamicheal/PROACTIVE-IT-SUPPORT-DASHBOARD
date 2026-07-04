// Deploy Agent Page Logic
let activeRegToken = '';
let countdownTimer = null;
let pollTimer = null;
let allUsers = [];

document.addEventListener('DOMContentLoaded', async () => {
    const generateTokenBtn = document.getElementById('generateTokenBtn');
    const copyBtn = document.getElementById('copyBtn');
    const downloadBtn = document.getElementById('downloadBtn');
    const assignmentTypeSelect = document.getElementById('assignmentTypeSelect');

    // Load existing employees
    if (assignmentTypeSelect) {
        loadEmployees();
        
        // Show/hide fields conditionally
        assignmentTypeSelect.addEventListener('change', (e) => {
            const val = e.target.value;
            const existingContainer = document.getElementById('existingUserContainer');
            const newContainer = document.getElementById('newUserContainer');
            
            if (val === 'existing') {
                existingContainer.classList.remove('hidden');
                newContainer.classList.add('hidden');
            } else if (val === 'new') {
                existingContainer.classList.add('hidden');
                newContainer.classList.remove('hidden');
            } else {
                existingContainer.classList.add('hidden');
                newContainer.classList.add('hidden');
            }
        });
    }

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

async function loadEmployees() {
    const userDropdown = document.getElementById('userDropdown');
    if (!userDropdown) return;
    
    try {
        allUsers = await api.getDeviceUsers() || [];
        userDropdown.innerHTML = '<option value="">-- Choose Employee --</option>';
        if (allUsers.length === 0) {
            userDropdown.innerHTML = '<option value="">No employees registered yet</option>';
            return;
        }
        allUsers.forEach(u => {
            const opt = document.createElement('option');
            opt.value = u.id;
            opt.textContent = `${u.full_name} (${u.email}) - ${u.department || 'No Dept'}`;
            userDropdown.appendChild(opt);
        });
    } catch (err) {
        console.error('Failed to load device users:', err);
        userDropdown.innerHTML = '<option value="">Failed to load employees</option>';
    }
}

async function generateDeploymentToken() {
    const step1Pane = document.getElementById('step1Pane');
    const step2Pane = document.getElementById('step2Pane');
    const tokenText = document.getElementById('tokenText');
    const statusDot = document.getElementById('statusDot');
    const statusLabel = document.getElementById('statusLabel');
    const assignmentType = document.getElementById('assignmentTypeSelect').value;

    let assignedUserId = null;

    try {
        if (assignmentType === 'existing') {
            const selectedVal = document.getElementById('userDropdown').value;
            if (!selectedVal) {
                alert('Please select an employee to assign.');
                return;
            }
            assignedUserId = parseInt(selectedVal);
        } else if (assignmentType === 'new') {
            const fullName = document.getElementById('newFullName').value.trim();
            const email = document.getElementById('newEmail').value.trim();
            const department = document.getElementById('newDepartment').value.trim();

            if (!fullName || !email) {
                alert('Please fill out all required fields: Full Name and Email.');
                return;
            }

            // Create new employee first
            const newUser = await api.createDeviceUser(fullName, email, department);
            assignedUserId = newUser.id;
        }

        const res = await api.createRegistrationToken(assignedUserId);
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
    window.location.href = api.getBaseOrigin() + '/api/deploy/installer';
}
