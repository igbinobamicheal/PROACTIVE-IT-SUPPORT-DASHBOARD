document.addEventListener('DOMContentLoaded', async () => {
    const token = localStorage.getItem('jwt_token');
    const username = localStorage.getItem('username') || 'Admin Console';
    if (!token) {
        window.location.href = 'login.html';
        return;
    }

    // Set username display
    const userDisplay = document.getElementById('currentUser');
    if (userDisplay) userDisplay.textContent = username;

    let alertsList = [];
    let filterActiveOnly = false;
    let eventSource = null;

    // Load initial alerts
    await fetchAlerts();
    initializeSSE();

    // Event listeners for filters
    const btnAll = document.getElementById('btnFilterAll');
    const btnActive = document.getElementById('btnFilterActive');

    // Initial button classes
    updateFilterButtonUI();

    if (btnAll && btnActive) {
        btnAll.addEventListener('click', () => {
            if (!filterActiveOnly) return;
            filterActiveOnly = false;
            updateFilterButtonUI();
            renderAlertsTable();
        });

        btnActive.addEventListener('click', () => {
            if (filterActiveOnly) return;
            filterActiveOnly = true;
            updateFilterButtonUI();
            renderAlertsTable();
        });
    }

    function updateFilterButtonUI() {
        if (!btnAll || !btnActive) return;
        if (filterActiveOnly) {
            btnActive.className = "px-2.5 py-1 text-[11px] font-medium rounded bg-white/[0.08] text-textMain shadow-sm";
            btnAll.className = "px-2.5 py-1 text-[11px] font-medium rounded text-textMuted hover:text-textMain transition-colors";
        } else {
            btnAll.className = "px-2.5 py-1 text-[11px] font-medium rounded bg-white/[0.08] text-textMain shadow-sm";
            btnActive.className = "px-2.5 py-1 text-[11px] font-medium rounded text-textMuted hover:text-textMain transition-colors";
        }
    }

    async function fetchAlerts() {
        try {
            // Fetch all alerts
            const data = await api.getAlerts(false);
            alertsList = data || [];
            updateStatsCards();
            renderAlertsTable();
        } catch (err) {
            console.error('Failed to fetch alerts:', err);
            const tbody = document.getElementById('alertsTableBody');
            if (tbody) {
                tbody.innerHTML = `<tr><td colspan="6" class="text-center text-danger py-6 text-[12px] font-semibold">Failed to load alerts: ${escapeHtml(err.message)}</td></tr>`;
            }
        }
    }

    function updateStatsCards() {
        const firing = alertsList.filter(a => !a.resolved).length;
        const resolved = alertsList.filter(a => a.resolved).length;

        const firingEl = document.getElementById('firingCount');
        const resolvedEl = document.getElementById('resolvedCount');

        if (firingEl) firingEl.textContent = firing;
        if (resolvedEl) resolvedEl.textContent = resolved;
    }

    function renderAlertsTable() {
        const tbody = document.getElementById('alertsTableBody');
        if (!tbody) return;
        tbody.innerHTML = '';

        let displayedAlerts = alertsList;
        if (filterActiveOnly) {
            displayedAlerts = alertsList.filter(a => !a.resolved);
        }

        if (displayedAlerts.length === 0) {
            tbody.innerHTML = `<tr><td colspan="6" class="text-center text-textMuted py-8 text-[12px] font-medium">No alerts found matching the criteria.</td></tr>`;
            return;
        }

        displayedAlerts.forEach(a => {
            const tr = document.createElement('tr');
            tr.id = `alert-row-${a.id}`;
            tr.className = `border-b border-white/[0.02] hover:bg-white/[0.02] transition-colors ${a.resolved ? 'opacity-60' : 'row-critical bg-danger/[0.02]'}`;

            const stateBadge = a.resolved
                ? `<div class="inline-flex items-center gap-1 px-2 py-0.5 rounded border border-success/30 bg-success/10 text-success text-[10px] font-bold uppercase tracking-wider">
                      <i data-lucide="check" class="w-3 h-3"></i> Resolved
                   </div>`
                : `<div class="inline-flex items-center gap-1.5 px-2 py-0.5 rounded border border-danger/30 bg-danger/10 text-danger text-[10px] font-bold uppercase tracking-wider">
                      <div class="w-1 h-1 rounded-full bg-danger animate-pulse shadow-[0_0_8px_rgba(244,63,94,0.6)]"></div> Firing
                   </div>`;

            tr.innerHTML = `
                <td class="py-3 px-4"><input type="checkbox" class="rounded bg-bg border-borderSubtle accent-primary"></td>
                <td class="py-3 px-4">${stateBadge}</td>
                <td class="py-3 px-4">
                    <div class="font-medium text-textMain text-[13px] mb-0.5">${escapeHtml(a.message)}</div>
                    <div class="flex items-center gap-2">
                        <i data-lucide="server" class="w-3 h-3 text-textSubtle"></i>
                        <a href="device-details.html?id=${a.device_id}" class="text-[11px] text-primary hover:underline font-mono">${escapeHtml(a.device_name)}</a>
                    </div>
                </td>
                <td class="py-3 px-4 font-mono">
                    <span class="${a.resolved ? 'text-textMuted' : 'text-danger font-semibold'}">${a.rule_value.toFixed(1)}%</span> 
                    <span class="text-textSubtle">/ >${a.threshold.toFixed(1)}%</span>
                </td>
                <td class="py-3 px-4 font-mono text-textMuted text-[11px]">
                    ${escapeHtml(a.timestamp)}
                </td>
                <td class="py-3 px-4 text-right">
                    <div class="flex items-center justify-end gap-2">
                        ${!a.resolved ? `<span class="text-danger text-[10px] font-mono px-1.5 py-0.5 bg-danger/10 rounded border border-danger/20">Triggered</span>` : `<span class="text-success text-[10px] font-mono px-1.5 py-0.5 bg-success/10 rounded border border-success/20">Resolved</span>`}
                    </div>
                </td>
            `;
            tbody.appendChild(tr);
        });

        // Initialize Lucide icons
        if (window.lucide) {
            window.lucide.createIcons();
        }
    }

    function initializeSSE() {
        if (eventSource) {
            eventSource.close();
        }

        const eventsUrl = `${api.getBaseOrigin()}/api/events?token=${encodeURIComponent(token)}`;
        eventSource = new EventSource(eventsUrl);

        eventSource.onopen = () => {
            console.log('[SSE] Alerts page subscriber connected.');
        };

        eventSource.onerror = (err) => {
            console.error('[SSE] Connection error on alerts page.', err);
        };

        // Listen for new alerts
        eventSource.addEventListener('new_alert', (e) => {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] New Alert Event:', data);

                // Add to list if it doesn't exist
                const exists = alertsList.some(a => a.id === data.id);
                if (!exists) {
                    alertsList.unshift(data); // Prepend to top
                    updateStatsCards();

                    // If filtering and it fits, or not filtering, render it!
                    if (!filterActiveOnly || !data.resolved) {
                        renderAlertsTable();
                        
                        // Apply flash transition to the new row
                        const tr = document.getElementById(`alert-row-${data.id}`);
                        if (tr) {
                            tr.style.backgroundColor = 'rgba(244, 63, 94, 0.1)';
                            setTimeout(() => {
                                tr.style.transition = 'background-color 1s ease';
                                tr.style.backgroundColor = 'transparent';
                            }, 100);
                        }
                    }
                }
            } catch (err) {
                console.error('[SSE] Error handling new_alert:', err);
            }
        });

        // Listen for alert resolutions
        eventSource.addEventListener('alert_resolved', (e) => {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] Alert Resolved Event:', data);

                // Find active alerts matching this device and rule type, and mark them resolved
                alertsList.forEach(a => {
                    if (a.device_id === data.device_id && a.rule_type === data.rule_type && !a.resolved) {
                        a.resolved = true;
                    }
                });

                updateStatsCards();

                if (filterActiveOnly) {
                    // If filtering active only, fade out and remove resolved rows
                    renderAlertsTable();
                } else {
                    renderAlertsTable();
                }
            } catch (err) {
                console.error('[SSE] Error handling alert_resolved:', err);
            }
        });
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
});
