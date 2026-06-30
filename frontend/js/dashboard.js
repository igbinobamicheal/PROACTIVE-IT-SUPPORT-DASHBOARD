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



    let devicesList = [];
    let activeAlertsList = [];
    const latestMetricsMap = new Map(); // deviceId -> latest metrics object

    let cpuChart = null;
    let ramChart = null;
    let diskChart = null;

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

        // Fetch global trends
        try {
            const trends = await api.getMetricTrends();
            updateCharts(trends || []);
        } catch (err) {
            console.error('Failed to fetch global trends:', err);
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

    function updateCharts(trends) {
        const hasData = trends && trends.length > 0;
        
        // Show/hide empty state overlays
        toggleEmptyState('cpuChart', !hasData);
        toggleEmptyState('ramChart', !hasData);
        toggleEmptyState('diskChart', !hasData);
        
        if (!hasData) return;

        // Format labels: extract HH:MM from hour_bucket (format: YYYY-MM-DD HH:MM:SS)
        const labels = trends.map(t => {
            const parts = t.hour_bucket.split(' ');
            if (parts.length > 1) {
                return parts[1].substring(0, 5); // "HH:MM"
            }
            return t.hour_bucket;
        });

        const cpuData = trends.map(t => t.avg_cpu);
        const ramData = trends.map(t => t.avg_ram);
        const diskData = trends.map(t => t.avg_disk);

        if (!cpuChart) {
            cpuChart = createLineChart('cpuChart', labels, cpuData, '#B87C3B', 'rgba(184, 124, 59, 0.1)');
        } else {
            cpuChart.data.labels = labels;
            cpuChart.data.datasets[0].data = cpuData;
            cpuChart.update();
        }

        if (!ramChart) {
            ramChart = createLineChart('ramChart', labels, ramData, '#10B981', 'rgba(16, 185, 129, 0.06)');
        } else {
            ramChart.data.labels = labels;
            ramChart.data.datasets[0].data = ramData;
            ramChart.update();
        }

        if (!diskChart) {
            diskChart = createLineChart('diskChart', labels, diskData, '#64748B', 'rgba(100, 116, 139, 0.05)');
        } else {
            diskChart.data.labels = labels;
            diskChart.data.datasets[0].data = diskData;
            diskChart.update();
        }
    }

    function toggleEmptyState(chartId, showEmpty) {
        const canvas = document.getElementById(chartId);
        const emptyEl = document.getElementById(`${chartId}Empty`);
        if (!canvas || !emptyEl) return;
        
        if (showEmpty) {
            canvas.classList.add('hidden');
            emptyEl.classList.remove('hidden');
        } else {
            canvas.classList.remove('hidden');
            emptyEl.classList.add('hidden');
        }
    }

    function createLineChart(canvasId, labels, data, color, fillGradStart) {
        const ctx = document.getElementById(canvasId);
        if (!ctx) return null;
        
        const gradient = ctx.getContext('2d').createLinearGradient(0, 0, 0, 200);
        gradient.addColorStop(0, fillGradStart);
        gradient.addColorStop(1, 'rgba(255, 255, 255, 0)');

        Chart.defaults.color = '#7C7770';
        Chart.defaults.font.family = 'JetBrains Mono';
        Chart.defaults.font.size = 10;

        return new Chart(ctx.getContext('2d'), {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    data: data,
                    borderColor: color,
                    backgroundColor: gradient,
                    fill: true,
                    tension: 0.2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: { legend: { display: false } },
                scales: {
                    x: { display: true, grid: { display: false }, border: { display: false }, ticks: { maxTicksLimit: 6 } },
                    y: { 
                        display: true, 
                        position: 'right',
                        grid: { color: 'rgba(30, 26, 23, 0.04)' },
                        border: { display: false },
                        min: 0,
                        max: 100
                    }
                },
                elements: { point: { radius: 0, hoverRadius: 4 }, line: { borderWidth: 1.5 } },
                interaction: { intersect: false, mode: 'index' }
            }
        });
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
            item.className = 'flex items-center justify-between p-3 rounded-md border border-danger/15 bg-danger/5';
            item.innerHTML = `
                <div class="flex items-center gap-3">
                    <span class="inline-flex items-center px-2 py-0.5 rounded bg-danger/5 text-danger text-[10px] font-bold uppercase tracking-wider border border-danger/15">${escapeHtml(a.rule_type.toUpperCase())}</span>
                    <span class="text-[12px] font-semibold text-textMain">${escapeHtml(a.device_name)}</span>
                    <span class="text-[12px] text-textMuted">&mdash; ${escapeHtml(a.message)}</span>
                </div>
                <div class="text-[11px] text-danger font-mono font-semibold">
                    ${formatTimeOnly(a.timestamp)}
                </div>
            `;
            listContainer.appendChild(item);
        });
        if (window.lucide) {
            window.lucide.createIcons();
        }
    }

    function renderRecentDevicesTable() {
        const tbody = document.getElementById('recentDevicesBody');
        if (!tbody) return;
        tbody.innerHTML = '';

        if (devicesList.length === 0) {
            tbody.innerHTML = `<tr><td colspan="4" class="text-center text-textMuted py-4">No agents registered yet.</td></tr>`;
            return;
        }

        // Show last 5 devices
        const recent = [...devicesList].reverse().slice(0, 5);
        recent.forEach(d => {
            const tr = document.createElement('tr');
            tr.className = 'border-b border-black/[0.03] hover:bg-black/[0.015] transition-colors cursor-pointer';
            tr.onclick = () => {
                window.location.href = `device-details.html?id=${d.id}`;
            };
            
            const statusColor = d.status === 'online' 
                ? 'border-success/20 bg-success/5 text-success' 
                : 'border-danger/20 bg-danger/5 text-danger';

            tr.innerHTML = `
                <td class="py-2.5 px-4 font-semibold text-textMain font-mono">
                    <div>${escapeHtml(d.name)}</div>
                    <div class="text-[10px] text-textSubtle font-sans mt-0.5 font-normal">
                        ${d.assigned_user_name ? `Assigned to: ${escapeHtml(d.assigned_user_name)}` : '<span class="italic">Unassigned</span>'}
                    </div>
                </td>
                <td class="py-2.5 px-4 text-textMuted font-mono">${escapeHtml(d.ip_address)}</td>
                <td class="py-2.5 px-4">
                    <span class="inline-flex items-center px-2 py-0.5 rounded ${statusColor} text-[10px] font-bold uppercase tracking-wider border">
                        ${d.status.toUpperCase()}
                    </span>
                </td>
                <td class="py-2.5 px-4 text-right" onclick="event.stopPropagation()">
                    <a href="device-details.html?id=${d.id}" class="inline-flex items-center px-2.5 py-1 rounded bg-black/[0.02] border border-borderSubtle text-textMain hover:bg-black/[0.04] text-[11px] font-medium transition-colors">Details</a>
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
            tbody.innerHTML = `<tr><td colspan="4" class="text-center text-textMuted py-4">All monitoring agents are offline.</td></tr>`;
            return;
        }

        onlineDevices.forEach(d => {
            const m = latestMetricsMap.get(d.id);
            const tr = document.createElement('tr');
            tr.id = `metrics-row-${d.id}`;
            tr.className = 'border-b border-black/[0.03] hover:bg-black/[0.015] transition-colors cursor-pointer';
            tr.onclick = () => {
                window.location.href = `device-details.html?id=${d.id}`;
            };
            
            if (m) {
                tr.innerHTML = `
                    <td class="py-2.5 px-4 font-semibold text-textMain font-mono">${escapeHtml(d.name)}</td>
                    <td class="py-2.5 px-4">
                        <div class="flex items-center gap-2">
                            <span class="text-[12px] font-medium text-textMain font-mono">${m.cpu_usage.toFixed(1)}%</span>
                            <div class="custom-progress">
                                <div class="custom-progress-bar bg-primary" style="width: ${m.cpu_usage}%"></div>
                            </div>
                        </div>
                    </td>
                    <td class="py-2.5 px-4">
                        <div class="flex items-center gap-2">
                            <span class="text-[12px] font-medium text-textMain font-mono">${m.ram_usage.toFixed(1)}%</span>
                            <div class="custom-progress">
                                <div class="custom-progress-bar bg-success" style="width: ${m.ram_usage}%"></div>
                            </div>
                        </div>
                    </td>
                    <td class="py-2.5 px-4 font-medium text-textMain font-mono">${m.disk_usage.toFixed(1)}%</td>
                `;
            } else {
                tr.innerHTML = `
                    <td class="py-2.5 px-4 font-semibold text-textMain font-mono">${escapeHtml(d.name)}</td>
                    <td colspan="3" class="py-2.5 px-4 text-center text-textMuted italic font-mono">
                        Connecting...
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
                    row.style.backgroundColor = 'rgba(184, 124, 59, 0.08)';
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

        const indicatorColor = type === 'danger' ? 'text-danger' : 'text-success';

        const toast = document.createElement('div');
        toast.className = 'bg-card text-textMain rounded-lg shadow-lg border border-borderSubtle p-4 min-w-[320px] max-w-[420px] pointer-events-auto transform translate-x-0 opacity-100 transition-all duration-500';
        toast.innerHTML = `
            <div class="flex items-start justify-between gap-3">
                <div class="flex-1">
                    <div class="text-[11px] font-mono uppercase tracking-wider mb-1 flex items-center gap-1.5 ${indicatorColor}">
                        <i data-lucide="${type === 'danger' ? 'alert-triangle' : 'check-circle-2'}" class="w-3.5 h-3.5"></i>
                        ${escapeHtml(title)}
                    </div>
                    <div class="text-[12px] font-medium text-textMuted">${escapeHtml(message)}</div>
                </div>
                <button class="text-textSubtle hover:text-textMain transition-colors flex-shrink-0 mt-0.5" onclick="this.parentElement.parentElement.remove()">
                    <i data-lucide="x" class="w-3.5 h-3.5"></i>
                </button>
            </div>
        `;
        
        container.appendChild(toast);
        if (window.lucide) {
            window.lucide.createIcons();
        }
        
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
