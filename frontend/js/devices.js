// Devices & Token Management Page Logic
let currentRegToken = '';
let modalTimer = null;
let modalPollInterval = null;

document.addEventListener('DOMContentLoaded', async () => {
    // 1. Initial Load of Devices (org-tree)
    loadDevices();

    // 2. Tab Navigation Binding
    const tabDevicesBtn = document.getElementById('tabDevicesBtn');
    const tabTokensBtn = document.getElementById('tabTokensBtn');
    const devicesListSection = document.getElementById('devicesListSection');
    const tokensListSection = document.getElementById('tokensListSection');

    if (tabDevicesBtn && tabTokensBtn) {
        tabDevicesBtn.addEventListener('click', () => {
            tabDevicesBtn.className = 'pb-2 border-b-2 border-primary text-textMain font-semibold transition-all';
            tabTokensBtn.className = 'pb-2 border-b-2 border-transparent hover:text-textMain transition-all';
            devicesListSection.classList.remove('hidden');
            tokensListSection.classList.add('hidden');
            loadDevices();
        });

        tabTokensBtn.addEventListener('click', () => {
            tabTokensBtn.className = 'pb-2 border-b-2 border-primary text-textMain font-semibold transition-all';
            tabDevicesBtn.className = 'pb-2 border-b-2 border-transparent hover:text-textMain transition-all';
            tokensListSection.classList.remove('hidden');
            devicesListSection.classList.add('hidden');
            loadTokens();
        });
    }

    // 3. Add Device Button Binding
    const addDeviceBtn = document.getElementById('addDeviceBtn');
    if (addDeviceBtn) {
        addDeviceBtn.addEventListener('click', () => {
            openAddDeviceModal();
        });
    }

    // 4. Auto-open wizard modal if requested via query string
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams.get('add') === 'true') {
        openAddDeviceModal();
    }
});

// ==========================================
// Org-Tree: Load & Render Devices
// ==========================================
async function loadDevices() {
    const container = document.getElementById('orgTreeContainer');
    if (!container) return;

    try {
        const devices = await api.getDevices();
        container.innerHTML = '';

        if (!devices || devices.length === 0) {
            container.innerHTML = `
                <div class="text-center py-16 flex flex-col items-center gap-3">
                    <div class="w-14 h-14 rounded-full bg-black/[0.03] flex items-center justify-center">
                        <i data-lucide="server" class="w-6 h-6 text-textSubtle"></i>
                    </div>
                    <div>
                        <div class="text-[14px] font-semibold text-textMain">No devices registered yet</div>
                        <div class="text-[12px] text-textSubtle mt-1">Click "Add Device" to generate a registration token and onboard your first device.</div>
                    </div>
                </div>`;
            window.lucide.createIcons();
            return;
        }

        // Group devices by department
        const groups = {};
        devices.forEach(d => {
            const dept = d.assigned_user_dept || '';
            const key = dept || '__unassigned__';
            if (!groups[key]) groups[key] = [];
            groups[key].push(d);
        });

        // Sort department names alphabetically, unassigned last
        const sortedDepts = Object.keys(groups).sort((a, b) => {
            if (a === '__unassigned__') return 1;
            if (b === '__unassigned__') return -1;
            return a.localeCompare(b);
        });

        // Render each department node
        sortedDepts.forEach((deptKey, index) => {
            const isUnassigned = deptKey === '__unassigned__';
            const deptName = isUnassigned ? 'Unassigned Devices' : deptKey;
            const deptDevices = groups[deptKey];
            const onlineCount = deptDevices.filter(d => d.status === 'online').length;
            const totalCount = deptDevices.length;

            const node = document.createElement('div');
            node.className = 'dept-node bg-surface border border-borderSubtle rounded-xl glass-panel shadow-sm animate-fade-in-up';
            node.style.animationDelay = `${index * 0.05}s`;

            // Determine avatar colors based on department
            const deptColors = getDeptColor(deptKey);

            node.innerHTML = `
                <!-- Department Header -->
                <div class="dept-header flex items-center justify-between px-5 py-3.5 rounded-t-xl" onclick="toggleDeptNode(this)">
                    <div class="flex items-center gap-3">
                        <div class="w-9 h-9 rounded-lg ${deptColors.bg} flex items-center justify-center flex-shrink-0">
                            <i data-lucide="${isUnassigned ? 'inbox' : 'building-2'}" class="w-4.5 h-4.5 ${deptColors.text}"></i>
                        </div>
                        <div>
                            <div class="flex items-center gap-2">
                                <span class="dept-name font-semibold text-[14px] text-textMain tracking-tight">${escapeHtml(deptName)}</span>
                                ${!isUnassigned ? `<button class="text-textSubtle hover:text-primary transition-colors p-0.5" onclick="event.stopPropagation(); startRenameDept(this, '${escapeHtml(deptName)}')" title="Rename Department">
                                    <i data-lucide="pencil" class="w-3 h-3"></i>
                                </button>` : ''}
                            </div>
                            <div class="text-[11px] text-textSubtle mt-0.5">
                                ${totalCount} ${totalCount === 1 ? 'device' : 'devices'}
                                <span class="mx-1 text-borderSubtle">•</span>
                                <span class="${onlineCount > 0 ? 'text-success' : 'text-textSubtle'}">${onlineCount} online</span>
                            </div>
                        </div>
                    </div>
                    <div class="flex items-center gap-2">
                        <i data-lucide="chevron-down" class="w-4 h-4 text-textSubtle dept-chevron"></i>
                    </div>
                </div>

                <!-- Department Body (Device Cards Grid) -->
                <div class="dept-body px-5 pb-4 pt-1">
                    <div class="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-3">
                        ${deptDevices.map(d => renderDeviceCard(d)).join('')}
                    </div>
                </div>
            `;

            container.appendChild(node);
        });

        // Initialize Lucide icons on newly created elements
        window.lucide.createIcons();

    } catch (err) {
        container.innerHTML = `
            <div class="text-center py-12 text-danger font-semibold text-[13px]">
                <i data-lucide="alert-triangle" class="w-5 h-5 mx-auto mb-2"></i>
                Failed to load devices: ${escapeHtml(err.message)}
            </div>`;
        window.lucide.createIcons();
    }
}

// Render a single device card
function renderDeviceCard(d) {
    const isOnline = d.status === 'online';
    const statusDot = isOnline
        ? 'bg-success shadow-[0_0_6px_rgba(16,185,129,0.4)]'
        : 'bg-zinc-300';
    const statusText = isOnline ? 'Online' : 'Offline';

    const displayName = d.assigned_user_name || d.name;
    const initial = displayName.charAt(0).toUpperCase();
    const subtitle = d.assigned_user_name
        ? `${escapeHtml(d.name)}`
        : 'No user assigned';

    const avatarBg = d.assigned_user_name ? 'bg-primary/10 text-primary' : 'bg-zinc-100 text-zinc-400';

    return `
        <div class="device-card bg-white/40 rounded-lg p-3.5 cursor-pointer flex flex-col gap-3" onclick="window.location.href='device-details.html?id=${d.id}'">
            <!-- Top Row: Avatar + Name + Status -->
            <div class="flex items-start justify-between">
                <div class="flex items-center gap-2.5 min-w-0">
                    <div class="w-8 h-8 rounded-full ${avatarBg} flex items-center justify-center text-[13px] font-bold flex-shrink-0">
                        ${initial}
                    </div>
                    <div class="min-w-0">
                        <div class="font-semibold text-[13px] text-textMain truncate">${escapeHtml(displayName)}</div>
                        <div class="text-[10px] text-textSubtle truncate font-mono">${subtitle}</div>
                    </div>
                </div>
                <div class="flex items-center gap-1.5 flex-shrink-0 mt-0.5">
                    <div class="w-1.5 h-1.5 rounded-full ${statusDot}"></div>
                    <span class="text-[10px] text-textSubtle font-medium">${statusText}</span>
                </div>
            </div>

            <!-- Info Row -->
            <div class="flex items-center gap-3 text-[10px] text-textSubtle font-mono">
                <span>${escapeHtml(d.ip_address)}</span>
                <span class="text-borderSubtle">•</span>
                <span class="truncate">${d.mac_address ? escapeHtml(d.mac_address) : 'N/A'}</span>
            </div>

            <!-- Bottom Row: Actions -->
            <div class="flex items-center gap-2 pt-1 border-t border-black/[0.04]" onclick="event.stopPropagation()">
                ${d.assigned_user_name
                    ? `<button onclick="openAssignModal(${d.id})" class="text-[10px] text-textSubtle hover:text-primary font-medium transition-colors flex items-center gap-1" title="Reassign / Edit">
                           <i data-lucide="user-cog" class="w-3 h-3"></i> Edit User
                       </button>`
                    : `<button onclick="openAssignModal(${d.id})" class="text-[10px] text-primary hover:text-primaryHover font-semibold transition-colors flex items-center gap-1">
                           <i data-lucide="user-plus" class="w-3 h-3"></i> Assign User
                       </button>`
                }
                <span class="text-borderSubtle">|</span>
                <a href="device-details.html?id=${d.id}" class="text-[10px] text-textSubtle hover:text-primary font-medium transition-colors flex items-center gap-1">
                    <i data-lucide="activity" class="w-3 h-3"></i> Metrics
                </a>
            </div>
        </div>
    `;
}

// Color scheme per department (deterministic based on name hash)
function getDeptColor(deptKey) {
    if (deptKey === '__unassigned__') {
        return { bg: 'bg-zinc-100', text: 'text-zinc-400' };
    }
    const palettes = [
        { bg: 'bg-primary/10', text: 'text-primary' },
        { bg: 'bg-emerald-500/10', text: 'text-emerald-600' },
        { bg: 'bg-blue-500/10', text: 'text-blue-600' },
        { bg: 'bg-violet-500/10', text: 'text-violet-600' },
        { bg: 'bg-rose-500/10', text: 'text-rose-600' },
        { bg: 'bg-amber-500/10', text: 'text-amber-600' },
        { bg: 'bg-cyan-500/10', text: 'text-cyan-600' },
        { bg: 'bg-fuchsia-500/10', text: 'text-fuchsia-600' },
    ];
    let hash = 0;
    for (let i = 0; i < deptKey.length; i++) {
        hash = ((hash << 5) - hash) + deptKey.charCodeAt(i);
        hash |= 0;
    }
    return palettes[Math.abs(hash) % palettes.length];
}

// ==========================================
// Org-Tree: Expand / Collapse
// ==========================================
function toggleDeptNode(headerEl) {
    const node = headerEl.closest('.dept-node');
    const body = node.querySelector('.dept-body');
    const chevron = node.querySelector('.dept-chevron');

    if (body.classList.contains('collapsed')) {
        body.classList.remove('collapsed');
        chevron.classList.remove('collapsed');
    } else {
        body.classList.add('collapsed');
        chevron.classList.add('collapsed');
    }
}

// ==========================================
// Org-Tree: Inline Department Rename
// ==========================================
function startRenameDept(btnEl, currentName) {
    const node = btnEl.closest('.dept-node');
    const nameSpan = node.querySelector('.dept-name');

    // Replace span with input
    const input = document.createElement('input');
    input.type = 'text';
    input.value = currentName;
    input.className = 'dept-rename-input';
    input.style.width = Math.max(120, currentName.length * 9) + 'px';

    nameSpan.replaceWith(input);
    input.focus();
    input.select();

    const commitRename = async () => {
        const newName = input.value.trim();
        if (!newName || newName === currentName) {
            // Revert
            const span = document.createElement('span');
            span.className = 'dept-name font-semibold text-[14px] text-textMain tracking-tight';
            span.textContent = currentName;
            input.replaceWith(span);
            return;
        }

        try {
            await api.renameDepartment(currentName, newName);
            loadDevices(); // Refresh the entire tree
        } catch (e) {
            alert('Failed to rename department: ' + e.message);
            const span = document.createElement('span');
            span.className = 'dept-name font-semibold text-[14px] text-textMain tracking-tight';
            span.textContent = currentName;
            input.replaceWith(span);
        }
    };

    input.addEventListener('blur', commitRename);
    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            e.preventDefault();
            input.blur();
        }
        if (e.key === 'Escape') {
            input.value = currentName; // Reset so blur reverts
            input.blur();
        }
    });
}

// ==========================================
// Registration Tokens
// ==========================================
async function loadTokens() {
    const tableBody = document.getElementById('tokensTableBody');
    if (!tableBody) return;

    try {
        const tokens = await api.getRegistrationTokens();
        tableBody.innerHTML = '';

        if (tokens.length === 0) {
            tableBody.innerHTML = `<tr><td colspan="5" class="py-8 text-center text-textMuted font-medium">No registration tokens generated yet.</td></tr>`;
            return;
        }

        tokens.forEach(t => {
            const tr = document.createElement('tr');
            tr.className = 'border-b border-black/[0.03] text-textMuted';

            let statusBadge = '';
            let isActionable = false;

            if (t.used) {
                statusBadge = `<span class="px-2 py-0.5 rounded bg-emerald-50 text-emerald-600 border border-emerald-200/50 text-[10px] font-semibold">Registered</span>`;
            } else if (t.is_expired) {
                statusBadge = `<span class="px-2 py-0.5 rounded bg-zinc-100 text-zinc-500 border border-zinc-200/50 text-[10px] font-semibold">Expired / Revoked</span>`;
            } else {
                statusBadge = `<span class="px-2 py-0.5 rounded bg-amber-50 text-amber-600 border border-amber-200/50 text-[10px] font-semibold animate-pulse">Pending</span>`;
                isActionable = true;
            }

            const revokeBtn = isActionable
                ? `<button onclick="revokeToken('${t.token}')" class="text-danger hover:underline text-[11px] font-semibold">Revoke</button>`
                : `<span class="text-textSubtle/50 cursor-not-allowed">Revoked</span>`;

            tr.innerHTML = `
                <td class="py-3 px-4">
                    <div class="font-mono text-textMain break-all">${escapeHtml(t.token)}</div>
                    <div class="text-[10px] text-textSubtle mt-0.5">Token ID: ${t.id}</div>
                </td>
                <td class="py-3 px-4">
                    ${statusBadge}
                </td>
                <td class="py-3 px-4">
                    ${t.assigned_user_name
                        ? `<div class="font-semibold text-textMain">${escapeHtml(t.assigned_user_name)}</div>
                           <div class="text-[10px] text-textSubtle">${escapeHtml(t.assigned_user_email)}</div>`
                        : `<span class="text-textSubtle italic">Unassigned</span>`}
                </td>
                <td class="py-3 px-4 font-mono text-[11px]">
                    ${t.used ? 'Completed Registration' : (t.is_expired ? 'Expired / Revoked' : 'Waiting for Agent')}
                </td>
                <td class="py-3 px-4 flex items-center gap-3">
                    <button onclick="copyTokenToClipboard('${t.token}', this)" class="text-primary hover:underline text-[11px] font-semibold">Copy</button>
                    <span class="text-borderSubtle">|</span>
                    ${revokeBtn}
                </td>
            `;
            tableBody.appendChild(tr);
        });
    } catch (err) {
        tableBody.innerHTML = `<tr><td colspan="5" class="py-8 text-center text-danger font-semibold">Failed to load registration tokens: ${escapeHtml(err.message)}</td></tr>`;
    }
}

// Revoke Token Handler
async function revokeToken(tokenStr) {
    if (!confirm('Are you sure you want to revoke this registration token? Agents will no longer be able to use it to onboard.')) {
        return;
    }
    try {
        await api.revokeRegistrationToken(tokenStr);
        loadTokens();
    } catch (err) {
        alert('Failed to revoke token: ' + err.message);
    }
}

// Copy Token Helper
function copyTokenToClipboard(tokenText, btnEl) {
    navigator.clipboard.writeText(tokenText).then(() => {
        const originalText = btnEl.textContent;
        btnEl.textContent = 'Copied!';
        btnEl.classList.add('text-success');
        setTimeout(() => {
            btnEl.textContent = originalText;
            btnEl.classList.remove('text-success');
        }, 2000);
    });
}

// ==========================================
// Add Device Wizard
// ==========================================
async function openAddDeviceModal() {
    document.getElementById('addDeviceModal').classList.remove('hidden');
    document.getElementById('modalStep1').classList.remove('hidden');
    document.getElementById('modalStep2').classList.add('hidden');
    document.getElementById('modalStep3').classList.add('hidden');

    // Clear onboarding user inputs
    document.getElementById('modalUserFullName').value = '';
    document.getElementById('modalUserEmail').value = '';
    document.getElementById('modalUserDept').value = '';
}

function closeAddDeviceModal(shouldRefresh = false) {
    document.getElementById('addDeviceModal').classList.add('hidden');
    if (modalTimer) clearInterval(modalTimer);
    if (modalPollInterval) clearInterval(modalPollInterval);
    if (shouldRefresh) {
        loadDevices();
    }
}

// Generate token from the Wizard modal
async function modalGenerateToken() {
    try {
        const fullName = document.getElementById('modalUserFullName').value.trim();
        const email = document.getElementById('modalUserEmail').value.trim();
        const dept = document.getElementById('modalUserDept').value.trim();

        let assignedUserId = null;
        if (email) {
            if (!fullName) {
                alert('Full Name is required if Staff Email is provided.');
                return;
            }
            const user = await api.ensureDeviceUser(fullName, email, dept);
            if (user && user.id) {
                assignedUserId = user.id;
            }
        }

        const res = await api.createRegistrationToken(assignedUserId);
        currentRegToken = res.token;

        // Switch to Step 2
        document.getElementById('modalStep1').classList.add('hidden');
        document.getElementById('modalStep2').classList.remove('hidden');
        document.getElementById('modalTokenText').textContent = currentRegToken;

        // Initialize Lucide icons on new block
        window.lucide.createIcons();

        // 15-minute countdown clock
        startModalCountdown(15 * 60);

        // Start polling tokens list to check if token is marked used!
        startTokenUsagePolling(currentRegToken);

    } catch (e) {
        alert('Failed to generate registration token: ' + e.message);
    }
}

function startModalCountdown(durationSeconds) {
    if (modalTimer) clearInterval(modalTimer);

    let timeRemaining = durationSeconds;
    const countdownEl = document.getElementById('modalCountdownText');

    const updateTimer = () => {
        const minutes = Math.floor(timeRemaining / 60);
        const seconds = timeRemaining % 60;
        countdownEl.textContent = `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;

        if (timeRemaining <= 0) {
            clearInterval(modalTimer);
            if (modalPollInterval) clearInterval(modalPollInterval);
            countdownEl.textContent = "EXPIRED";
            alert('Registration token has expired. Please generate a new one.');
            closeAddDeviceModal();
        }
        timeRemaining--;
    };

    updateTimer();
    modalTimer = setInterval(updateTimer, 1000);
}

// Poll tokens backend to check if the generated token gets marked used
function startTokenUsagePolling(tokenStr) {
    if (modalPollInterval) clearInterval(modalPollInterval);

    modalPollInterval = setInterval(async () => {
        try {
            const tokens = await api.getRegistrationTokens();
            const currentToken = tokens.find(t => t.token === tokenStr);

            if (currentToken && currentToken.used) {
                // Device registered! Switch to Step 3
                clearInterval(modalPollInterval);
                if (modalTimer) clearInterval(modalTimer);

                document.getElementById('modalStep2').classList.add('hidden');
                document.getElementById('modalStep3').classList.remove('hidden');
                window.lucide.createIcons();
            }
        } catch (e) {
            console.error('Failed to poll token status:', e);
        }
    }, 4000);
}

function modalCopyToken() {
    if (!currentRegToken) return;
    navigator.clipboard.writeText(currentRegToken).then(() => {
        const btn = document.getElementById('modalCopyBtn');
        const text = document.getElementById('modalCopyText');
        text.textContent = 'Copied!';
        btn.classList.replace('bg-surface', 'bg-success/15');
        btn.classList.add('text-success');

        setTimeout(() => {
            text.textContent = 'Copy';
            btn.classList.replace('bg-success/15', 'bg-surface');
            btn.classList.remove('text-success');
        }, 2500);
    });
}

function modalDownloadInstaller() {
    const apiBase = window.API_BASE_URL || 'http://localhost:8080/api';
    window.location.href = `${apiBase}/deploy/installer`;
}

// ==========================================
// Utility
// ==========================================
function escapeHtml(str) {
    if (!str) return '';
    return str
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

// ==========================================
// Device Assignment Modal Control
// ==========================================
let activeAssignDeviceId = null;

function openAssignModal(deviceId) {
    activeAssignDeviceId = deviceId;

    const modalEl = document.getElementById('assignmentModal');
    if (!modalEl) return;

    modalEl.classList.remove('hidden');

    // Reset inputs
    document.getElementById('newFullName').value = '';
    document.getElementById('newEmail').value = '';
    document.getElementById('newDepartment').value = '';

    // Pre-populate if device already has an assigned user
    api.getDevices().then(devices => {
        const d = devices.find(x => x.id === deviceId);
        if (d && d.assigned_user_name) {
            document.getElementById('newFullName').value = d.assigned_user_name;
            document.getElementById('newEmail').value = d.assigned_user_email || '';
            document.getElementById('newDepartment').value = d.assigned_user_dept || '';
        }
    }).catch(e => console.error('Failed to pre-populate assignment modal:', e));
}

function closeAssignmentModal() {
    const modalEl = document.getElementById('assignmentModal');
    if (modalEl) {
        modalEl.classList.add('hidden');
    }
    activeAssignDeviceId = null;
}

// Bind modal controls on load
document.addEventListener('DOMContentLoaded', () => {
    const closeBtn = document.getElementById('closeModalBtn');
    if (closeBtn) {
        closeBtn.addEventListener('click', () => closeAssignmentModal());
    }

    // Close modal when clicking backdrop
    const modalEl = document.getElementById('assignmentModal');
    if (modalEl) {
        modalEl.addEventListener('click', (e) => {
            if (e.target === modalEl) closeAssignmentModal();
        });
    }

    // Auto lookup user name/dept by email on email field blur
    const emailInput = document.getElementById('newEmail');
    if (emailInput) {
        emailInput.addEventListener('change', async (e) => {
            const email = e.target.value.trim();
            if (!email) return;
            try {
                const users = await api.getDeviceUsers();
                const matched = users.find(u => u.email.toLowerCase() === email.toLowerCase());
                if (matched) {
                    document.getElementById('newFullName').value = matched.full_name;
                    document.getElementById('newDepartment').value = matched.department || '';
                }
            } catch (err) {
                console.error('Email auto-lookup failed:', err);
            }
        });
    }

    const btnAssignUser = document.getElementById('submitAssignUser');
    if (btnAssignUser) {
        btnAssignUser.addEventListener('click', async () => {
            const fullName = document.getElementById('newFullName').value.trim();
            const email = document.getElementById('newEmail').value.trim();
            const dept = document.getElementById('newDepartment').value.trim();

            if (!fullName || !email) {
                alert('Full Name and Staff Email are required.');
                return;
            }

            // Disable button during operation
            btnAssignUser.disabled = true;
            btnAssignUser.textContent = 'Saving...';

            try {
                // Ensure user exists or is updated via upsert
                const user = await api.ensureDeviceUser(fullName, email, dept);
                if (user && user.id) {
                    const updated = await api.assignDeviceUser(activeAssignDeviceId, user.id);
                    if (updated) {
                        loadDevices();
                        closeAssignmentModal();
                    }
                }
            } catch (e) {
                alert('Failed to assign user: ' + e.message);
            } finally {
                btnAssignUser.disabled = false;
                btnAssignUser.textContent = 'Assign & Save User';
            }
        });
    }
});
