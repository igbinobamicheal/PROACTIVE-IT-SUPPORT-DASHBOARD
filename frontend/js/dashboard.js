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

    // Generate Agent Package Button Handler
    const generateBtn = document.getElementById('generateAgentBtn');
    if (generateBtn) {
        generateBtn.addEventListener('click', async () => {
            try {
                generateBtn.disabled = true;
                generateBtn.innerHTML = `<span class="w-4 h-4 border-2 border-white border-t-transparent rounded-full animate-spin inline-block mr-2"></span>Generating...`;

                const response = await fetch(`${api.getBaseOrigin()}/api/deploy/agent`, {
                    method: 'GET',
                    headers: {
                        'Authorization': `Bearer ${token}`
                    }
                });

                if (!response.ok) {
                    let errMsg = 'Failed to generate agent package';
                    try {
                        const errData = await response.json();
                        errMsg = errData.error || errMsg;
                    } catch (e) {}
                    throw new Error(errMsg);
                }

                const blob = await response.blob();
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'agent_package.zip';
                document.body.appendChild(a);
                a.click();
                a.remove();
                window.URL.revokeObjectURL(url);

                showToast('Success', 'Agent deployment package downloaded successfully.', 'success');
            } catch (err) {
                console.error(err);
                showToast('Deployment Error', err.message, 'danger');
            } finally {
                generateBtn.disabled = false;
                generateBtn.innerHTML = `<span class="material-symbols-outlined text-[18px]">download</span> Generate Agent Package`;
            }
        });
    }

    let devicesList = [];
    let activeAlertsList = [];
    const latestMetricsMap = new Map(); // deviceId -> latest metrics object

    // Initial Load
    try {
        await refreshDashboard();
        initializeSSE();
    } catch (err) {
        console.error('Failed to load dashboard:', err);
    }

    async function refreshDashboard() {
        // Fetch devices
        devicesList = await api.getDevices();
        if (!devicesList) return;

        // Fetch active alerts
        try {
            const alerts = await api.getAlerts(true);
            activeAlertsList = alerts || [];
        } catch (err) {
            console.error('Failed to fetch active alerts:', err);
        }

        // Fetch latest metrics for online devices
        const onlineDevices = devicesList.filter(d => d.status === 'online');
        await Promise.all(onlineDevices.map(async (d) => {
            try {
                const detailedDev = await api.getDevice(d.id);
                if (detailedDev && detailedDev.latest_metrics) {
                    latestMetricsMap.set(d.id, detailedDev.latest_metrics);
                }
            } catch (err) {
                console.error(`Failed to fetch metrics for device ${d.id}:`, err);
            }
        }));

        updateUI();
    }

    function updateUI() {
        updateStatsCards();
        renderActiveAlertsPanel();
        renderRecentDevicesTable();
        renderLatestMetricsTable();
    }

    function updateStatsCards() {
        const total = devicesList.length;
        const online = devicesList.filter(d => d.status === 'online').length;
        const offline = total - online;
        const alertsCount = activeAlertsList.length;

        document.getElementById('totalCount').textContent = total;
        document.getElementById('onlineCount').textContent = online;
        document.getElementById('offlineCount').textContent = offline;
        document.getElementById('alertCount').textContent = alertsCount;
    }

    function renderActiveAlertsPanel() {
        const panel = document.getElementById('activeAlertsPanel');
        const badge = document.getElementById('activeAlertsBadge');
        const listContainer = document.getElementById('activeAlertsList');
        if (!panel || !badge || !listContainer) return;

        if (activeAlertsList.length === 0) {
            panel.classList.add('hidden');
            return;
        }

        panel.classList.remove('hidden');
        badge.textContent = activeAlertsList.length;
        listContainer.innerHTML = '';

        activeAlertsList.forEach(a => {
            const item = document.createElement('div');
            item.className = 'flex items-center justify-between p-3.5 rounded-2xl border border-red-200 bg-red-50';
            item.innerHTML = `
                <div class="flex items-center gap-3">
                    <span class="inline-flex items-center px-2.5 py-1 rounded-full bg-red-600 text-white text-[10px] font-black uppercase tracking-wider">${escapeHtml(a.rule_type)}</span>
                    <span class="text-[14px] font-bold text-red-950">${escapeHtml(a.device_name)}</span>
                    <span class="text-[13px] text-red-800/70 font-medium">&mdash; ${escapeHtml(a.message)}</span>
                </div>
                <div class="text-[11px] text-red-400 font-semibold">
                    ${formatTimeOnly(a.timestamp)}
                </div>
            `;
            listContainer.appendChild(item);
        });
    }

    function renderRecentDevicesTable() {
        const tbody = document.getElementById('recentDevicesBody');
        if (!tbody) return;
        tbody.innerHTML = '';

        if (devicesList.length === 0) {
            tbody.innerHTML = `<tr><td colspan="4" class="text-center text-[#0c0f0a]/40 py-6 text-[14px]">No agents registered yet.</td></tr>`;
            return;
        }

        // Show last 5 devices
        const recent = [...devicesList].reverse().slice(0, 5);
        recent.forEach(d => {
            const tr = document.createElement('tr');
            tr.className = 'border-b border-[#f9f9f9] hover:bg-[#f9f9f9]/50 transition-colors';
            const statusColor = d.status === 'online' ? 'bg-green-50 text-green-700 border-green-200' : 'bg-red-50 text-red-700 border-red-200';
            tr.innerHTML = `
                <td class="py-4 px-6 font-semibold text-[#0c0f0a]">${escapeHtml(d.name)}</td>
                <td class="py-4 px-6 text-[#0c0f0a]/60">${escapeHtml(d.ip_address)}</td>
                <td class="py-4 px-6">
                    <span class="inline-flex items-center px-3 py-1 rounded-full ${statusColor} text-[11px] font-black uppercase tracking-wider border">
                        ${d.status.toUpperCase()}
                    </span>
                </td>
                <td class="py-4 px-6 text-right">
                    <a href="device-details.html?id=${d.id}" class="inline-flex items-center px-4 py-2 rounded-full bg-[#f9f9f9] text-[#0c0f0a] text-[12px] font-bold border border-[#0c0f0a]/10 hover:bg-[#0c0f0a] hover:text-white transition-all duration-300">Details</a>
                </td>
            `;
            tbody.appendChild(tr);
        });
    }

    function renderLatestMetricsTable() {
        const tbody = document.getElementById('latestMetricsBody');
        if (!tbody) return;
        tbody.innerHTML = '';

        const onlineDevices = devicesList.filter(d => d.status === 'online');
        if (onlineDevices.length === 0) {
            tbody.innerHTML = `<tr><td colspan="5" class="text-center text-[#0c0f0a]/40 py-6 text-[14px]">All monitoring agents are currently offline.</td></tr>`;
            return;
        }

        onlineDevices.forEach(d => {
            const m = latestMetricsMap.get(d.id);
            const tr = document.createElement('tr');
            tr.id = `metrics-row-${d.id}`;
            tr.className = 'border-b border-[#f9f9f9] hover:bg-[#f9f9f9]/50 transition-colors';
            
            if (m) {
                tr.innerHTML = `
                    <td class="py-4 px-6 font-semibold text-[#0c0f0a]">${escapeHtml(d.name)}</td>
                    <td class="py-4 px-6">
                        <div class="flex items-center gap-2">
                            <span class="text-[14px] font-medium text-[#0c0f0a]">${m.cpu_usage.toFixed(1)}%</span>
                            <div class="custom-progress">
                                <div class="custom-progress-bar bg-blue" style="width: ${m.cpu_usage}%"></div>
                            </div>
                        </div>
                    </td>
                    <td class="py-4 px-6">
                        <div class="flex items-center gap-2">
                            <span class="text-[14px] font-medium text-[#0c0f0a]">${m.ram_usage.toFixed(1)}%</span>
                            <div class="custom-progress">
                                <div class="custom-progress-bar bg-purple" style="width: ${m.ram_usage}%"></div>
                            </div>
                        </div>
                    </td>
                    <td class="py-4 px-6 text-[14px] font-medium text-[#0c0f0a]">${m.disk_usage.toFixed(1)}%</td>
                    <td class="py-4 px-6 text-[13px] text-[#0c0f0a]/50">${formatUptime(m.uptime)}</td>
                `;
            } else {
                tr.innerHTML = `
                    <td class="py-4 px-6 font-semibold text-[#0c0f0a]">${escapeHtml(d.name)}</td>
                    <td colspan="4" class="py-4 px-6 text-center text-[#0c0f0a]/40 text-[13px] italic">
                        Connecting... Waiting for first metric push
                    </td>
                `;
            }
            tbody.appendChild(tr);
        });
    }

    // Initialize EventSource
    let eventSource = null;
    function initializeSSE() {
        if (eventSource) {
            eventSource.close();
        }

        const eventsUrl = `${api.getBaseOrigin()}/api/events?token=${encodeURIComponent(token)}`;
        eventSource = new EventSource(eventsUrl);

        eventSource.onopen = () => {
            console.log('[SSE] Connection to events channel established.');
        };

        eventSource.onerror = (err) => {
            console.error('[SSE] Connection error. Reconnecting...', err);
        };

        // Event Listeners
        eventSource.addEventListener('device_online', (e) => {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] Device Online Event:', data);
                
                const dev = devicesList.find(d => d.id === data.device_id);
                if (dev) {
                    dev.status = 'online';
                } else {
                    devicesList.push({
                        id: data.device_id,
                        name: data.device_name,
                        ip_address: data.ip_address,
                        status: 'online'
                    });
                }
                updateUI();
            } catch (err) {
                console.error('[SSE] Error handling device_online:', err);
            }
        });

        eventSource.addEventListener('device_offline', (e) => {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] Device Offline Event:', data);
                
                const dev = devicesList.find(d => d.id === data.device_id);
                if (dev) {
                    dev.status = 'offline';
                }
                latestMetricsMap.delete(data.device_id);
                
                // Offline is considered an alert resolver if metric alerts drop
                activeAlertsList = activeAlertsList.filter(a => a.device_id !== data.device_id);

                updateUI();
            } catch (err) {
                console.error('[SSE] Error handling device_offline:', err);
            }
        });

        eventSource.addEventListener('metric', (e) => {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] Metric Event:', data);
                
                const dev = devicesList.find(d => d.id === data.device_id);
                if (dev && dev.status === 'offline') {
                    dev.status = 'online';
                }

                latestMetricsMap.set(data.device_id, {
                    cpu_usage: data.cpu_usage,
                    ram_usage: data.ram_usage,
                    disk_usage: data.disk_usage,
                    network_in: data.network_in,
                    network_out: data.network_out,
                    uptime: data.uptime,
                    timestamp: data.timestamp
                });

                updateUI();
                
                // Add flash animation
                const row = document.getElementById(`metrics-row-${data.device_id}`);
                if (row) {
                    row.style.transition = 'none';
                    row.style.backgroundColor = 'rgba(255, 219, 15, 0.15)';
                    setTimeout(() => {
                        row.style.transition = 'background-color 1s ease';
                        row.style.backgroundColor = 'transparent';
                    }, 100);
                }
            } catch (err) {
                console.error('[SSE] Error handling metric event:', err);
            }
        });

        // Listen for new alerts from the Alert Engine
        eventSource.addEventListener('new_alert', (e) => {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] New Alert Event:', data);

                // Add to active alerts list if not present
                const exists = activeAlertsList.some(a => a.id === data.id);
                if (!exists) {
                    activeAlertsList.unshift(data); // Prepend to top
                    updateUI();

                    // Trigger toast notification
                    showToast(
                        `CRITICAL ALERT: ${data.device_name}`,
                        `${data.message}`,
                        'danger'
                    );
                }
            } catch (err) {
                console.error('[SSE] Error handling new_alert event:', err);
            }
        });

        // Listen for alert resolutions
        eventSource.addEventListener('alert_resolved', (e) => {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] Alert Resolved Event:', data);

                // Remove from active alerts
                const initialLength = activeAlertsList.length;
                activeAlertsList = activeAlertsList.filter(
                    a => !(a.device_id === data.device_id && a.rule_type === data.rule_type)
                );

                if (activeAlertsList.length < initialLength) {
                    updateUI();

                    // Trigger success toast notification
                    showToast(
                        `Alert Resolved: ${data.device_name}`,
                        `The ${data.rule_type.toUpperCase()} utilization has returned to normal levels.`,
                        'success'
                    );
                }
            } catch (err) {
                console.error('[SSE] Error handling alert_resolved event:', err);
            }
        });
    }

    function showToast(title, message, type = 'danger') {
        const container = document.getElementById('toastContainer');
        if (!container) return;

        const bgColor = type === 'danger' 
            ? 'bg-red-600' 
            : 'bg-green-600';
        const borderColor = type === 'danger'
            ? 'border-red-500'
            : 'border-green-500';

        const toast = document.createElement('div');
        toast.className = `${bgColor} text-white rounded-2xl shadow-2xl border ${borderColor} p-4 min-w-[320px] max-w-[420px] pointer-events-auto transform translate-x-0 opacity-100 transition-all duration-500`;
        toast.innerHTML = `
            <div class="flex items-start justify-between gap-3">
                <div class="flex-1">
                    <div class="text-[13px] font-black uppercase tracking-wider mb-1">${escapeHtml(title)}</div>
                    <div class="text-[13px] font-medium text-white/80">${escapeHtml(message)}</div>
                </div>
                <button class="text-white/60 hover:text-white transition-colors flex-shrink-0 mt-0.5" onclick="this.parentElement.parentElement.remove()">
                    <span class="material-symbols-outlined text-[18px]">close</span>
                </button>
            </div>
        `;
        
        container.appendChild(toast);
        
        // Auto remove after 5 seconds
        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transform = 'translateX(100%)';
            setTimeout(() => toast.remove(), 500);
        }, 5000);
    }

    function formatUptime(seconds) {
        const d = Math.floor(seconds / (3600*24));
        const h = Math.floor((seconds % (3600*24)) / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        
        let out = '';
        if (d > 0) out += `${d}d `;
        if (h > 0 || d > 0) out += `${h}h `;
        out += `${m}m`;
        return out;
    }

    function formatTimeOnly(ts) {
        if (!ts) return '';
        const parts = ts.split(' ');
        if (parts.length > 1) {
            return parts[1];
        }
        return ts;
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
