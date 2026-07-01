// Devices & Token Management Page Logic
let currentRegToken = '';
let modalTimer = null;
let modalPollInterval = null;

document.addEventListener('DOMContentLoaded', async () => {
    // 1. Initial Load of Devices
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

    // 4. Sort Selector Binding
    const sortSelect = document.getElementById('sortSelect');
    if (sortSelect) {
        sortSelect.addEventListener('change', () => {
            loadDevices();
        });
    }

    // 5. Auto-open wizard modal if requested via query string
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams.get('add') === 'true') {
        openAddDeviceModal();
    }
});

// Load Registered Devices
async function loadDevices() {
    const tableBody = document.getElementById('devicesTableBody');
    if (!tableBody) return;

    try {
        let devices = await api.getDevices();
        tableBody.innerHTML = '';
        
        if (devices.length === 0) {
            tableBody.innerHTML = `<tr><td colspan="6" class="py-8 text-center text-textMuted font-medium">No devices registered yet.</td></tr>`;
            return;
        }

        // Apply sorting
        const sortVal = document.getElementById('sortSelect') ? document.getElementById('sortSelect').value : 'none';
        if (sortVal === 'dept-asc') {
            devices.sort((a, b) => {
                const deptA = a.assigned_user_dept || '';
                const deptB = b.assigned_user_dept || '';
                return deptA.localeCompare(deptB);
            });
        } else if (sortVal === 'dept-desc') {
            devices.sort((a, b) => {
                const deptA = a.assigned_user_dept || '';
                const deptB = b.assigned_user_dept || '';
                return deptB.localeCompare(deptA);
            });
        } else if (sortVal === 'user-asc') {
            devices.sort((a, b) => {
                const nameA = a.assigned_user_name || '';
                const nameB = b.assigned_user_name || '';
                return nameA.localeCompare(nameB);
            });
        } else if (sortVal === 'user-desc') {
            devices.sort((a, b) => {
                const nameA = a.assigned_user_name || '';
                const nameB = b.assigned_user_name || '';
                return nameB.localeCompare(nameA);
            });
        }

        devices.forEach(d => {
            const tr = document.createElement('tr');
            tr.className = 'border-b border-black/[0.03] hover:bg-black/[0.015] transition-colors cursor-pointer';
            tr.onclick = () => {
                window.location.href = `device-details.html?id=${d.id}`;
            };
            
            const statusDotColor = d.status === 'online' 
                ? 'bg-success shadow-[0_0_8px_rgba(16,185,129,0.3)]' 
                : 'bg-danger shadow-[0_0_8px_rgba(239,68,68,0.3)]';
            const statusText = d.status === 'online' ? 'Healthy' : 'Critical';

            const osDisplay = d.windows_version ? escapeHtml(d.windows_version) : 'Windows Agent';
            const macDisplay = d.mac_address ? escapeHtml(d.mac_address) : 'N/A';

            // Primary display is User Name if assigned, fallback to Device Name
            const primaryDisplayName = d.assigned_user_name ? d.assigned_user_name : d.name;
            const subDisplayName = d.assigned_user_name ? `${d.name} &bull; ID: ${d.id}` : `ID: ${d.id}`;

            tr.innerHTML = `
                <td class="py-3 px-4" onclick="event.stopPropagation()">
                    <input type="checkbox" class="rounded bg-white border-borderSubtle text-primary accent-primary cursor-pointer">
                </td>
                <td class="py-3 px-4">
                    <div class="font-semibold text-textMain">${escapeHtml(primaryDisplayName)}</div>
                    <div class="text-[10px] text-textSubtle mt-0.5 font-mono">${subDisplayName}</div>
                </td>
                <td class="py-3 px-4">
                    ${d.assigned_user_name 
                        ? `<div class="flex items-center gap-1.5">
                               <span class="font-semibold text-textMain">${escapeHtml(d.assigned_user_name)}</span>
                               <button onclick="event.stopPropagation(); openAssignModal(${d.id})" class="text-textSubtle hover:text-textMain inline-flex items-center p-0.5" title="Reassign User">
                                   <i data-lucide="user-cog" class="w-3.5 h-3.5"></i>
                               </button>
                           </div>
                           <div class="text-[10px] text-textSubtle">${escapeHtml(d.assigned_user_email)} ${d.assigned_user_dept ? `[${escapeHtml(d.assigned_user_dept)}]` : ''}</div>`
                        : `<button onclick="event.stopPropagation(); openAssignModal(${d.id})" class="text-primary hover:underline text-[11px] font-semibold inline-flex items-center gap-1">
                               <i data-lucide="user-plus" class="w-3.5 h-3.5"></i> Assign User
                           </button>`}
                </td>
                <td class="py-3 px-4">
                    <span class="flex items-center gap-2 text-textMuted">
                        <div class="w-1.5 h-1.5 rounded-full ${statusDotColor}"></div>
                        ${statusText}
                    </span>
                </td>
                <td class="py-3 px-4 font-mono text-textMuted leading-relaxed">
                    <div>${escapeHtml(d.ip_address)}</div>
                    <div class="text-[10px] text-textSubtle uppercase tracking-wide mt-0.5">${osDisplay} &bull; MAC: ${macDisplay}</div>
                </td>
                <td class="py-3 px-4" onclick="event.stopPropagation()">
                    <a href="device-details.html?id=${d.id}" class="inline-flex items-center px-2.5 py-1 rounded bg-black/[0.02] border border-borderSubtle text-textMain hover:bg-black/[0.04] text-[11px] font-medium transition-colors">
                        View Metrics
                    </a>
                </td>
            `;
            tableBody.appendChild(tr);
        });
    } catch (err) {
        tableBody.innerHTML = `<tr><td colspan="6" class="py-8 text-center text-danger font-semibold">Failed to load devices: ${escapeHtml(err.message)}</td></tr>`;
    }
}

// Load Registration Tokens (Pending / Expired / Revoked)
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

// Add Device Wizard Navigation
async function openAddDeviceModal() {
    document.getElementById('addDeviceModal').classList.remove('hidden');
    document.getElementById('modalStep1').classList.remove('hidden');
    document.getElementById('modalStep2').classList.add('hidden');
    document.getElementById('modalStep3').classList.add('hidden');

    // Populate user selection dropdown
    try {
        const users = await api.getDeviceUsers();
        const select = document.getElementById('modalAssignUserSelect');
        if (select) {
            select.innerHTML = '<option value="">-- Leave Unassigned --</option>';
            if (users && users.length > 0) {
                users.forEach(u => {
                    const opt = document.createElement('option');
                    opt.value = u.id;
                    opt.textContent = `${u.full_name} (${u.email}) [${u.department || 'No Dept'}]`;
                    select.appendChild(opt);
                });
            }
        }
    } catch (e) {
        console.error('Failed to load device users for onboarding:', e);
    }
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
        const select = document.getElementById('modalAssignUserSelect');
        const assignedUserId = select && select.value ? parseInt(select.value) : null;

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
    
    document.getElementById('tabSelectExisting').click();

    // Populate existing users
    api.getDeviceUsers().then(users => {
        const dropdown = document.getElementById('userDropdown');
        dropdown.innerHTML = '';
        if (!users || users.length === 0) {
            dropdown.innerHTML = '<option value="">-- No users registered yet --</option>';
        } else {
            users.forEach(u => {
                const opt = document.createElement('option');
                opt.value = u.id;
                opt.textContent = `${u.full_name} (${u.email}) [${u.department || 'No Dept'}]`;
                dropdown.appendChild(opt);
            });
        }
    }).catch(e => {
        console.error('Failed to load device users:', e);
    });
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

    const tabSelectExisting = document.getElementById('tabSelectExisting');
    const tabCreateNew = document.getElementById('tabCreateNew');
    const formSelectExisting = document.getElementById('formSelectExisting');
    const formCreateNew = document.getElementById('formCreateNew');

    if (tabSelectExisting && tabCreateNew) {
        tabSelectExisting.addEventListener('click', () => {
            tabSelectExisting.className = 'pb-2 border-b-2 border-primary text-primary transition-all font-semibold';
            tabCreateNew.className = 'pb-2 border-b-2 border-transparent text-textSubtle hover:text-textMain transition-all font-semibold';
            formSelectExisting.classList.remove('hidden');
            formCreateNew.classList.add('hidden');
        });

        tabCreateNew.addEventListener('click', () => {
            tabCreateNew.className = 'pb-2 border-b-2 border-primary text-primary transition-all font-semibold';
            tabSelectExisting.className = 'pb-2 border-b-2 border-transparent text-textSubtle hover:text-textMain transition-all font-semibold';
            formCreateNew.classList.remove('hidden');
            formSelectExisting.classList.add('hidden');
        });
    }

    const btnAssignExisting = document.getElementById('submitAssignExisting');
    if (btnAssignExisting) {
        btnAssignExisting.addEventListener('click', async () => {
            const userId = document.getElementById('userDropdown').value;
            if (!userId) {
                alert('Please select a user to assign.');
                return;
            }

            try {
                const updated = await api.assignDeviceUser(activeAssignDeviceId, parseInt(userId));
                if (updated) {
                    loadDevices();
                    closeAssignmentModal();
                }
            } catch (e) {
                alert('Failed to assign user: ' + e.message);
            }
        });
    }

    const btnCreateAndAssign = document.getElementById('submitCreateAndAssign');
    if (btnCreateAndAssign) {
        btnCreateAndAssign.addEventListener('click', async () => {
            const fullName = document.getElementById('newFullName').value.trim();
            const email = document.getElementById('newEmail').value.trim();
            const dept = document.getElementById('newDepartment').value.trim();

            if (!fullName || !email) {
                alert('Full Name and Email are required to create a user.');
                return;
            }

            try {
                const newUser = await api.createDeviceUser(fullName, email, dept);
                if (newUser && newUser.id) {
                    const updated = await api.assignDeviceUser(activeAssignDeviceId, newUser.id);
                    if (updated) {
                        loadDevices();
                        closeAssignmentModal();
                    }
                }
            } catch (e) {
                alert('Failed to create and assign user: ' + e.message);
            }
        });
    }
});
