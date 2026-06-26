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

    if (btnAll && btnActive) {
        btnAll.addEventListener('click', () => {
            if (!filterActiveOnly) return;
            filterActiveOnly = false;
            btnAll.classList.add('active');
            btnActive.classList.remove('active');
            renderAlertsTable();
        });

        btnActive.addEventListener('click', () => {
            if (filterActiveOnly) return;
            filterActiveOnly = true;
            btnActive.classList.add('active');
            btnAll.classList.remove('active');
            renderAlertsTable();
        });
    }

    async function fetchAlerts() {
        try {
            // Fetch all alerts (we'll filter in memory so we can update live states smoothly!)
            const data = await api.getAlerts(false);
            alertsList = data || [];
            renderAlertsTable();
        } catch (err) {
            console.error('Failed to fetch alerts:', err);
            const tbody = document.getElementById('alertsTableBody');
            if (tbody) {
                tbody.innerHTML = `<tr><td colspan="7" class="text-center text-red-600 py-6 text-[14px] font-semibold">Failed to load alerts: ${escapeHtml(err.message)}</td></tr>`;
            }
        }
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
            tbody.innerHTML = `<tr><td colspan="7" class="text-center text-[#0c0f0a]/40 py-6 text-[14px]">No alerts found matching the criteria.</td></tr>`;
            return;
        }

        displayedAlerts.forEach(a => {
            const tr = document.createElement('tr');
            tr.id = `alert-row-${a.id}`;
            tr.className = 'border-b border-[#f9f9f9] hover:bg-[#f9f9f9]/50 transition-all duration-300';

            const statusBadge = a.resolved
                ? '<span class="inline-flex items-center px-3 py-1 rounded-full bg-green-50 text-green-700 border border-green-200 text-[11px] font-black uppercase tracking-wider">RESOLVED</span>'
                : '<span class="inline-flex items-center px-3 py-1 rounded-full bg-red-50 text-red-700 border border-red-200 text-[11px] font-black uppercase tracking-wider">ACTIVE</span>';

            tr.innerHTML = `
                <td class="py-3.5 px-6 text-[#0c0f0a]/50 text-[13px] font-semibold">${escapeHtml(a.timestamp)}</td>
                <td class="py-3.5 px-6 text-[#0c0f0a] font-semibold">${escapeHtml(a.device_name)}</td>
                <td class="py-3.5 px-6"><span class="badge bg-secondary-glow">${escapeHtml(a.rule_type)}</span></td>
                <td class="py-3.5 px-6 font-semibold" style="color: ${a.resolved ? '#0c0f0a' : '#ba1a1a'}">${a.rule_value.toFixed(1)}%</td>
                <td class="py-3.5 px-6 text-[#0c0f0a]/50">${a.threshold.toFixed(1)}%</td>
                <td class="py-3.5 px-6 text-[#0c0f0a]">${escapeHtml(a.message)}</td>
                <td class="py-3.5 px-6" id="alert-status-cell-${a.id}">
                    ${statusBadge}
                </td>
            `;
            tbody.appendChild(tr);
        });
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

                    // If filtering and it fits, or not filtering, render it!
                    if (!filterActiveOnly || !data.resolved) {
                        renderAlertsTable();
                        
                        // Apply flash transition to the new row
                        const tr = document.getElementById(`alert-row-${data.id}`);
                        if (tr) {
                            tr.style.backgroundColor = 'rgba(186, 26, 26, 0.1)';
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
                        
                        if (filterActiveOnly) {
                            // If filtering active only, fade out and remove the row
                            const row = document.getElementById(`alert-row-${a.id}`);
                            if (row) {
                                row.style.transition = 'opacity 0.8s ease';
                                row.style.opacity = '0';
                                setTimeout(() => {
                                    renderAlertsTable();
                                }, 800);
                            }
                        } else {
                            // If showing all, update the status cell badge
                            const cell = document.getElementById(`alert-status-cell-${a.id}`);
                            if (cell) {
                                cell.innerHTML = `
                                    <span class="inline-flex items-center px-3 py-1 rounded-full bg-green-50 text-green-700 border border-green-200 text-[11px] font-black uppercase tracking-wider">
                                        RESOLVED
                                    </span>
                                `;
                                const row = document.getElementById(`alert-row-${a.id}`);
                                if (row) {
                                    row.style.backgroundColor = 'rgba(34, 197, 94, 0.1)';
                                    setTimeout(() => {
                                        row.style.transition = 'background-color 1s ease';
                                        row.style.backgroundColor = 'transparent';
                                    }, 100);
                                }
                            }
                        }
                    }
                });
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
