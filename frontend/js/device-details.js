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

    // Get Device ID from URL query parameters
    const urlParams = new URLSearchParams(window.location.search);
    const deviceId = parseInt(urlParams.get('id'));
    if (isNaN(deviceId)) {
        alert('Invalid device ID specified.');
        window.location.href = 'devices.html';
        return;
    }

    let cpuChart, ramChart, diskChart;
    let eventSource = null;
    
    // Globally cached diagnostic data
    let currentDiagnostics = null;
    let allProcesses = [];
    let allServices = [];
    let allLogs = [];
    let allInstalledApps = [];
    let allStartupApps = [];

    try {
        // 1. Fetch metadata and display
        const dev = await api.getDevice(deviceId);
        if (!dev) {
            alert('Device not found.');
            window.location.href = 'devices.html';
            return;
        }

        document.getElementById('deviceName').textContent = dev.name;
        document.getElementById('infoIp').textContent = dev.ip_address;
        document.getElementById('infoToken').textContent = dev.token;
        updateStatusBadge(dev.status);
        renderUserAssignment(dev);

        // 2. Fetch metrics history and initialize charts/table
        const history = await api.getDeviceMetrics(deviceId);
        initializeCharts(history);
        renderHistoryTable(history);

        // 3. Set initial stats card values based on latest metric in history
        if (history.length > 0) {
            const latest = history[history.length - 1];
            updateStatsCards(latest);
            document.getElementById('infoUptime').textContent = formatUptime(latest.uptime);
            document.getElementById('infoLastReport').textContent = formatTimeOnly(latest.timestamp);
        } else {
            document.getElementById('infoUptime').textContent = 'N/A';
            document.getElementById('infoLastReport').textContent = 'Never';
        }

        // 4. Fetch diagnostic data and render tabs
        await fetchAndRenderDiagnostics();

        // 5. Initialize real-time updates via SSE
        initializeSSE();

        // 6. Bind Search and Filter listeners
        bindSearchFilters();

        // 7. Bind Tabs Navigation
        bindTabsNavigation();

    } catch (err) {
        console.error('Failed to load device details:', err);
    }

    function updateStatusBadge(status) {
        const badge = document.getElementById('statusBadge');
        const text = document.getElementById('deviceStatusSub');
        if (!badge || !text) return;

        badge.textContent = status.toUpperCase();
        if (status === 'online') {
            badge.className = 'inline-flex items-center px-2.5 py-0.5 rounded border border-success/20 bg-success/5 text-success text-[10px] font-bold uppercase tracking-wider shadow-sm';
            text.innerHTML = '<span class="inline-block w-1.5 h-1.5 rounded-full bg-success shadow-[0_0_8px_rgba(16,185,129,0.3)] animate-pulse mr-2"></span><span class="text-[12px] text-textMuted font-medium">Active monitoring agent online</span>';
        } else if (status === 'restarting') {
            badge.className = 'inline-flex items-center px-2.5 py-0.5 rounded border border-warning/20 bg-warning/5 text-warning text-[10px] font-bold uppercase tracking-wider shadow-sm';
            text.innerHTML = '<span class="inline-block w-1.5 h-1.5 rounded-full bg-warning shadow-[0_0_8px_rgba(245,158,11,0.3)] animate-spin mr-2"></span><span class="text-[12px] text-textMuted font-medium">Agent is restarting...</span>';
        } else {
            badge.className = 'inline-flex items-center px-2.5 py-0.5 rounded border border-danger/20 bg-danger/5 text-danger text-[10px] font-bold uppercase tracking-wider shadow-sm';
            text.innerHTML = '<span class="inline-block w-1.5 h-1.5 rounded-full bg-danger shadow-[0_0_8px_rgba(239,68,68,0.3)] mr-2"></span><span class="text-[12px] text-textMuted font-medium">Agent offline (no contact)</span>';
        }
    }

    function updateStatsCards(m) {
        document.getElementById('cardCpu').textContent = `${m.cpu_usage.toFixed(1)}%`;
        document.getElementById('cardRam').textContent = `${m.ram_usage.toFixed(1)}%`;
        document.getElementById('cardDisk').textContent = `${m.disk_usage.toFixed(1)}%`;
        document.getElementById('cardNetwork').innerHTML = `
            <div class="flex items-center gap-1.5 mb-1 text-[11px] font-mono text-textMuted">
                <i data-lucide="arrow-down" class="w-3.5 h-3.5 text-primary"></i>
                <span>In:</span>
                <span class="text-textMain font-semibold">${m.network_in.toFixed(1)} MB</span>
            </div>
            <div class="flex items-center gap-1.5 text-[11px] font-mono text-textMuted">
                <i data-lucide="arrow-up" class="w-3.5 h-3.5 text-warning"></i>
                <span>Out:</span>
                <span class="text-textMain font-semibold">${m.network_out.toFixed(1)} MB</span>
            </div>
        `;
        if (window.lucide) {
            window.lucide.createIcons();
        }

        // Highlight alert thresholds (e.g. > 85%)
        highlightThreshold('cardCpu', m.cpu_usage);
        highlightThreshold('cardRam', m.ram_usage);
        highlightThreshold('cardDisk', m.disk_usage);
    }

    function highlightThreshold(elementId, value) {
        const el = document.getElementById(elementId);
        if (!el) return;
        const container = el.parentElement;
        if (value > 85) {
            el.className = 'text-[28px] font-bold font-mono tracking-tight text-danger';
            container.style.borderColor = 'rgba(239, 68, 68, 0.3)';
        } else if (value > 70) {
            el.className = 'text-[28px] font-bold font-mono tracking-tight text-warning';
            container.style.borderColor = 'rgba(184, 124, 59, 0.3)';
        } else {
            el.className = 'text-[28px] font-bold font-mono tracking-tight text-textMain';
            container.style.borderColor = 'rgba(30, 26, 23, 0.08)';
        }
    }

    function renderHistoryTable(history) {
        const tbody = document.getElementById('metricsHistoryBody');
        if (!tbody) return;
        tbody.innerHTML = '';

        if (history.length === 0) {
            tbody.innerHTML = `<tr><td colspan="7" class="text-center text-textMuted py-6 text-[12px] font-medium">No metrics history logged yet.</td></tr>`;
            return;
        }

        // Display newest reports first
        const sortedHistory = [...history].reverse();
        sortedHistory.forEach(m => {
            const tr = document.createElement('tr');
            tr.className = 'border-b border-black/[0.03] hover:bg-black/[0.015] transition-colors';
            tr.innerHTML = `
                <td class="py-3 px-4 font-semibold text-textMuted font-mono text-[11px]">${formatTimestamp(m.timestamp)}</td>
                <td class="py-3 px-4 font-mono"><span class="${m.cpu_usage > 85 ? 'text-danger font-bold' : 'text-textMain'}" class="font-medium">${m.cpu_usage.toFixed(1)}%</span></td>
                <td class="py-3 px-4 font-mono"><span class="${m.ram_usage > 85 ? 'text-danger font-bold' : 'text-textMain'}" class="font-medium">${m.ram_usage.toFixed(1)}%</span></td>
                <td class="py-3 px-4 font-mono text-textMain">${m.disk_usage.toFixed(1)}%</td>
                <td class="py-3 px-4 text-textMuted font-mono">${m.network_in.toFixed(2)} MB</td>
                <td class="py-3 px-4 text-textMuted font-mono">${m.network_out.toFixed(2)} MB</td>
                <td class="py-3 px-4 text-textMuted font-mono">${formatUptime(m.uptime)}</td>
            `;
            tbody.appendChild(tr);
        });
    }

    function initializeCharts(history) {
        const labels = history.map(m => formatTimeOnly(m.timestamp));
        const cpuData = history.map(m => m.cpu_usage);
        const ramData = history.map(m => m.ram_usage);
        const diskData = history.map(m => m.disk_usage);

        // Chart styling configurations
        const chartOptions = (title, accentGlow) => ({
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false },
                tooltip: {
                    mode: 'index',
                    intersect: false,
                    backgroundColor: '#ECEAE5',
                    titleColor: '#1E1A17',
                    bodyColor: '#4A4540',
                    borderColor: 'rgba(30, 26, 23, 0.08)',
                    borderWidth: 1,
                    titleFont: { family: 'Inter', weight: 'bold' },
                    bodyFont: { family: 'Inter' },
                    padding: 12,
                    cornerRadius: 12
                }
            },
            scales: {
                y: {
                    min: 0,
                    max: 100,
                    grid: { color: 'rgba(30, 26, 23, 0.04)' },
                    ticks: { color: '#7C7770', font: { family: 'Inter', size: 10, weight: 'bold' } }
                },
                x: {
                    grid: { display: false },
                    ticks: { color: 'rgba(30, 26, 23, 0.4)', font: { family: 'Inter', size: 10 } }
                }
            }
        });

        // Initialize CPU Line Chart (Amber)
        const cpuCtx = document.getElementById('cpuChart').getContext('2d');
        const cpuGrad = cpuCtx.createLinearGradient(0, 0, 0, 200);
        cpuGrad.addColorStop(0, 'rgba(184, 124, 59, 0.1)');
        cpuGrad.addColorStop(1, 'rgba(184, 124, 59, 0.0)');

        cpuChart = new Chart(cpuCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    data: cpuData,
                    borderColor: '#B87C3B',
                    backgroundColor: cpuGrad,
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.3,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: chartOptions('CPU', 'rgba(184, 124, 59, 0.3)')
        });

        // Initialize RAM Line Chart (Emerald)
        const ramCtx = document.getElementById('ramChart').getContext('2d');
        const ramGrad = ramCtx.createLinearGradient(0, 0, 0, 200);
        ramGrad.addColorStop(0, 'rgba(16, 185, 129, 0.06)');
        ramGrad.addColorStop(1, 'rgba(16, 185, 129, 0.0)');

        ramChart = new Chart(ramCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    data: ramData,
                    borderColor: '#10B981',
                    backgroundColor: ramGrad,
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.3,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: chartOptions('RAM', 'rgba(16, 185, 129, 0.3)')
        });

        // Initialize Disk Line Chart (Slate)
        const diskCtx = document.getElementById('diskChart').getContext('2d');
        const diskGrad = diskCtx.createLinearGradient(0, 0, 0, 200);
        diskGrad.addColorStop(0, 'rgba(100, 116, 139, 0.05)');
        diskGrad.addColorStop(1, 'rgba(100, 116, 139, 0.0)');

        diskChart = new Chart(diskCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    data: diskData,
                    borderColor: '#64748B',
                    backgroundColor: diskGrad,
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.3,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: chartOptions('Disk', 'rgba(100, 116, 139, 0.3)')
        });
    }

    function initializeSSE() {
        if (eventSource) {
            eventSource.close();
        }

        const eventsUrl = `${api.getBaseOrigin()}/api/events?token=${encodeURIComponent(token)}`;
        eventSource = new EventSource(eventsUrl);

        eventSource.onopen = () => {
            console.log('[SSE] Device details listener started.');
        };

        eventSource.onerror = (err) => {
            console.error('[SSE] Device details channel error.', err);
        };

        // Listen for online/offline triggers
        eventSource.addEventListener('device_online', (e) => {
            const data = JSON.parse(e.data);
            if (data.device_id === deviceId) {
                updateStatusBadge('online');
            }
        });

        eventSource.addEventListener('device_offline', (e) => {
            const data = JSON.parse(e.data);
            if (data.device_id === deviceId) {
                updateStatusBadge('offline');
            }
        });

        // Listen for user assignment changes
        eventSource.addEventListener('device_user_assigned', (e) => {
            const data = JSON.parse(e.data);
            if (data.device_id === deviceId) {
                console.log('[SSE] Device user assignment update received:', data);
                const updatedDev = {
                    assigned_user_id: data.assigned_user_id,
                    assigned_user_name: data.assigned_user_name,
                    assigned_user_email: data.assigned_user_email,
                    assigned_user_dept: data.assigned_user_dept
                };
                renderUserAssignment(updatedDev);
            }
        });

        // Listen for diagnostics notifications
        eventSource.addEventListener('device_diagnostics', (e) => {
            const data = JSON.parse(e.data);
            if (data.device_id === deviceId) {
                console.log('[SSE] Diagnostics update received. Re-fetching...');
                fetchAndRenderDiagnostics();
            }
        });

        // Listen for new metrics
        eventSource.addEventListener('metric', (e) => {
            const data = JSON.parse(e.data);
            if (data.device_id !== deviceId) return;

            console.log('[SSE] Received metric for this device:', data);

            // Update status & uptime
            updateStatusBadge('online');
            document.getElementById('infoUptime').textContent = formatUptime(data.uptime);
            document.getElementById('infoLastReport').textContent = formatTimeOnly(data.timestamp);

            // Update stats cards
            updateStatsCards(data);

            // Append to Charts
            const timeLabel = formatTimeOnly(data.timestamp);
            
            appendChartData(cpuChart, timeLabel, data.cpu_usage);
            appendChartData(ramChart, timeLabel, data.ram_usage);
            appendChartData(diskChart, timeLabel, data.disk_usage);

            // Append to table
            const tbody = document.getElementById('metricsHistoryBody');
            if (tbody) {
                if (tbody.rows.length === 1 && tbody.rows[0].cells.length === 1) {
                    tbody.innerHTML = '';
                }

                const tr = document.createElement('tr');
                tr.className = 'border-b border-black/[0.03] hover:bg-black/[0.015] transition-colors';
                tr.innerHTML = `
                    <td class="py-3 px-4 font-semibold text-textMuted font-mono text-[11px]">${formatTimestamp(data.timestamp)}</td>
                    <td class="py-3 px-4 font-mono"><span class="${data.cpu_usage > 85 ? 'text-danger font-bold' : 'text-textMain'}" class="font-medium">${data.cpu_usage.toFixed(1)}%</span></td>
                    <td class="py-3 px-4 font-mono"><span class="${data.ram_usage > 85 ? 'text-danger font-bold' : 'text-textMain'}" class="font-medium">${data.ram_usage.toFixed(1)}%</span></td>
                    <td class="py-3 px-4 font-mono text-textMain">${data.disk_usage.toFixed(1)}%</td>
                    <td class="py-3 px-4 text-textMuted font-mono">${data.network_in.toFixed(2)} MB</td>
                    <td class="py-3 px-4 text-textMuted font-mono">${data.network_out.toFixed(2)} MB</td>
                    <td class="py-3 px-4 text-textMuted font-mono">${formatUptime(data.uptime)}</td>
                `;
                
                tbody.insertBefore(tr, tbody.firstChild);

                tr.style.backgroundColor = 'rgba(184, 124, 59, 0.08)';
                setTimeout(() => {
                    tr.style.transition = 'background-color 1s ease';
                    tr.style.backgroundColor = 'transparent';
                }, 100);

                if (tbody.rows.length > 100) {
                    tbody.removeChild(tbody.lastChild);
                }
            }
        });
    }

    function appendChartData(chart, label, value) {
        if (!chart) return;
        chart.data.labels.push(label);
        chart.data.datasets[0].data.push(value);

        if (chart.data.datasets[0].data.length > 20) {
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
        }
        chart.update('none');
    }

    // Diagnostics Fetch & Rendering
    async function fetchAndRenderDiagnostics() {
        try {
            const data = await api.getDeviceDiagnostics(deviceId);
            if (data) {
                currentDiagnostics = data;
                renderDiagnostics();
            }
        } catch (e) {
            console.error('Failed to fetch diagnostics:', e);
        }
    }

    function safeParseJson(val) {
        if (!val) return null;
        if (typeof val === 'object') return val;
        try {
            return JSON.parse(val);
        } catch (e) {
            console.warn('Failed to parse diagnostics JSON string field:', e);
            return null;
        }
    }

    function renderDiagnostics() {
        if (!currentDiagnostics) return;

        // 1. System Tab rendering
        const sys = safeParseJson(currentDiagnostics.system_info);
        if (sys) {
            document.getElementById('sysManufacturer').textContent = sys.manufacturer || 'Unknown';
            document.getElementById('sysModel').textContent = sys.model || 'Unknown';
            document.getElementById('sysSerial').textContent = sys.serial_number || 'Unknown';
            document.getElementById('sysBios').textContent = sys.bios_version || 'Unknown';
            document.getElementById('sysEdition').textContent = sys.windows_edition || 'Unknown';
            document.getElementById('sysBuild').textContent = sys.windows_build || 'Unknown';
            document.getElementById('sysArch').textContent = sys.architecture || 'Unknown';
            document.getElementById('sysLastBoot').textContent = sys.last_boot_time || 'Unknown';
            document.getElementById('sysUser').textContent = sys.logged_in_user || 'Unknown';
            
            if (sys.last_boot_time && sys.last_boot_time !== 'Unknown') {
                try {
                    const boot = new Date(sys.last_boot_time.replace(/-/g, '/'));
                    const diff = Math.floor((new Date() - boot) / 1000);
                    if (diff > 0) {
                        document.getElementById('sysUptime').textContent = formatUptime(diff);
                    } else {
                        document.getElementById('sysUptime').textContent = 'Just booted';
                    }
                } catch {
                    document.getElementById('sysUptime').textContent = 'Unknown';
                }
            } else {
                document.getElementById('sysUptime').textContent = 'Unknown';
            }
        }

        const cpu = safeParseJson(currentDiagnostics.cpu_info);
        if (cpu) {
            document.getElementById('sysCpuFreq').textContent = cpu.frequency_mhz ? `${cpu.frequency_mhz} MHz` : 'N/A';
            
            const coresContainer = document.getElementById('cpuCoresContainer');
            coresContainer.innerHTML = '';
            if (cpu.cores_usage && cpu.cores_usage.length > 0) {
                cpu.cores_usage.forEach((usage, idx) => {
                    const coreDiv = document.createElement('div');
                    coreDiv.className = 'flex flex-col p-2 bg-black/5 rounded border border-borderSubtle';
                    coreDiv.innerHTML = `
                        <div class="flex justify-between text-[10px] font-semibold text-textSubtle font-mono">
                            <span>Core ${idx}</span>
                            <span>${usage.toFixed(0)}%</span>
                        </div>
                        <div class="w-full bg-black/10 rounded-full h-1.5 mt-1 overflow-hidden">
                            <div class="h-full rounded-full transition-all duration-500 ${usage > 85 ? 'bg-danger' : usage > 70 ? 'bg-warning' : 'bg-primary'}" style="width: ${Math.min(100, Math.max(0, usage))}%"></div>
                        </div>
                    `;
                    coresContainer.appendChild(coreDiv);
                });
            } else {
                coresContainer.innerHTML = '<div class="text-center text-textMuted text-[12px] col-span-4 py-2">No per-core details.</div>';
            }
        }

        const bat = safeParseJson(currentDiagnostics.battery_info);
        const batteryGrid = document.getElementById('batteryGrid');
        const batteryUnavailableMsg = document.getElementById('batteryUnavailableMsg');
        if (bat && bat.battery_available) {
            batteryGrid.classList.remove('hidden');
            batteryUnavailableMsg.classList.add('hidden');
            document.getElementById('batLevel').textContent = `${bat.percentage}%`;
            document.getElementById('batStatus').textContent = bat.charging_status || 'Unknown';
            
            if (bat.estimated_remaining_time_mins > 0) {
                document.getElementById('batTime').textContent = `${bat.estimated_remaining_time_mins} mins`;
            } else if (bat.estimated_remaining_time_mins === -1) {
                document.getElementById('batTime').textContent = 'Calculating...';
            } else {
                document.getElementById('batTime').textContent = 'N/A';
            }
            document.getElementById('batHealth').textContent = bat.battery_health || 'Unknown';
        } else {
            batteryGrid.classList.add('hidden');
            batteryUnavailableMsg.classList.remove('hidden');
        }

        const storage = safeParseJson(currentDiagnostics.storage_info);
        const storageTableBody = document.getElementById('storageTableBody');
        if (storageTableBody) {
            storageTableBody.innerHTML = '';
            if (storage && storage.length > 0) {
                storage.forEach(s => {
                    const tr = document.createElement('tr');
                    tr.className = 'border-b border-black/[0.03] hover:bg-black/[0.015] transition-colors';
                    tr.innerHTML = `
                        <td class="py-2.5 px-4 font-mono font-bold text-textMain">${escapeHtml(s.drive_letter)}</td>
                        <td class="py-2.5 px-4 text-textSubtle">${escapeHtml(s.drive_type || 'HDD')}</td>
                        <td class="py-2.5 px-4 font-mono">${s.total_capacity_gb.toFixed(1)} GB</td>
                        <td class="py-2.5 px-4 font-mono">${s.used_capacity_gb.toFixed(1)} GB</td>
                        <td class="py-2.5 px-4 font-mono text-textMuted">${s.free_capacity_gb.toFixed(1)} GB</td>
                        <td class="py-2.5 px-4 font-mono">
                            <div class="flex items-center gap-2">
                                <span class="font-bold w-10">${s.percent_used.toFixed(0)}%</span>
                                <div class="w-16 bg-black/10 rounded-full h-1.5 overflow-hidden">
                                    <div class="h-full rounded-full ${s.percent_used > 85 ? 'bg-danger' : 'bg-primary'}" style="width: ${s.percent_used}%"></div>
                                </div>
                            </div>
                        </td>
                        <td class="py-2.5 px-4"><span class="px-1.5 py-0.5 rounded text-[10px] font-bold ${s.smart_status.toLowerCase() === 'healthy' ? 'bg-success/10 text-success' : 'bg-warning/10 text-warning'}">${escapeHtml(s.smart_status)}</span></td>
                        <td class="py-2.5 px-4 text-textSubtle">${escapeHtml(s.disk_health_indicators || 'Healthy')}</td>
                    `;
                    storageTableBody.appendChild(tr);
                });
            } else {
                storageTableBody.innerHTML = '<tr><td colspan="8" class="text-center py-6 text-textMuted">No storage interfaces detected.</td></tr>';
            }
        }

        // 2. Network & Security Tab rendering
        const net = safeParseJson(currentDiagnostics.network_info);
        if (net) {
            document.getElementById('netLocalIp').textContent = net.local_ip || 'N/A';
            document.getElementById('netPublicIp').textContent = net.public_ip || 'N/A';
            document.getElementById('netMac').textContent = net.mac_address || 'N/A';
            document.getElementById('netHostname').textContent = net.hostname || 'N/A';
            document.getElementById('netGateway').textContent = net.default_gateway || 'N/A';
            
            let dnsStr = 'N/A';
            if (net.dns_servers && net.dns_servers.length > 0) {
                dnsStr = net.dns_servers.join(', ');
            }
            document.getElementById('netDns').textContent = dnsStr;
            document.getElementById('netAdapter').textContent = net.active_adapter || 'N/A';
            document.getElementById('netConnType').textContent = net.connection_type || 'N/A';
            document.getElementById('netLatency').textContent = net.latency_ms >= 0 ? `${net.latency_ms} ms` : 'N/A';
            
            const isConnected = net.internet_status === 'Connected';
            const statusBadge = document.getElementById('netInternetStatus');
            statusBadge.textContent = net.internet_status || 'Disconnected';
            statusBadge.className = isConnected 
                ? 'text-[10px] font-bold uppercase tracking-wider px-2 py-0.5 rounded border shadow-sm bg-success/5 text-success border-success/20'
                : 'text-[10px] font-bold uppercase tracking-wider px-2 py-0.5 rounded border shadow-sm bg-danger/5 text-danger border-danger/20';
        }

        const sec = safeParseJson(currentDiagnostics.security_info);
        if (sec) {
            const defSpan = document.getElementById('secDefender');
            const fwSpan = document.getElementById('secFirewall');
            const updSpan = document.getElementById('secUpdate');
            const bitSpan = document.getElementById('secBitlocker');
            const restartSpan = document.getElementById('secPendingRestart');

            defSpan.textContent = sec.windows_defender || 'Unknown';
            defSpan.className = `font-bold text-[14px] mt-1 ${sec.windows_defender === 'Active' ? 'text-success' : 'text-danger'}`;
            
            fwSpan.textContent = sec.firewall_status || 'Unknown';
            fwSpan.className = `font-bold text-[14px] mt-1 ${sec.firewall_status === 'Active' ? 'text-success' : 'text-warning'}`;
            
            updSpan.textContent = sec.windows_update_status || 'Unknown';
            updSpan.className = `font-bold text-[14px] mt-1 ${sec.windows_update_status === 'Running' || sec.windows_update_status === 'Stopped' ? 'text-textMain' : 'text-warning'}`;
            
            bitSpan.textContent = sec.bitlocker_status || 'Unknown';
            bitSpan.className = `font-bold text-[14px] mt-1 ${sec.bitlocker_status === 'FullyDecrypted' ? 'text-danger' : 'text-success'}`;
            
            restartSpan.textContent = sec.pending_restart ? 'YES (Pending System Reboot)' : 'No pending restart';
            restartSpan.className = `font-semibold text-[13px] mt-1 ${sec.pending_restart ? 'text-danger animate-pulse' : 'text-textSubtle'}`;
        }

        // 3. Processes & Services tab variables
        const proc = safeParseJson(currentDiagnostics.processes_info);
        if (proc) {
            allProcesses = proc.top_processes || [];
            allServices = proc.critical_services || [];
            document.getElementById('procCountText').textContent = `Total active processes: ${proc.running_process_count || 'Unknown'}`;
        }
        renderProcessesAndServicesTable();

        // 4. Event logs parsing
        allLogs = safeParseJson(currentDiagnostics.event_logs) || [];
        renderEventLogsTable();

        // 5. Installed software parsing
        allInstalledApps = safeParseJson(currentDiagnostics.installed_software) || [];
        allStartupApps = safeParseJson(currentDiagnostics.startup_applications) || [];
        renderSoftwareTable();

        if (window.lucide) {
            window.lucide.createIcons();
        }
    }

    function renderProcessesAndServicesTable() {
        const filterText = document.getElementById('procSearchInput').value.trim().toLowerCase();
        
        const procTableBody = document.getElementById('procTableBody');
        if (procTableBody) {
            procTableBody.innerHTML = '';
            const filteredProc = allProcesses.filter(p => 
                p.name.toLowerCase().includes(filterText) || 
                p.pid.toString().includes(filterText)
            );

            if (filteredProc.length > 0) {
                filteredProc.forEach(p => {
                    const tr = document.createElement('tr');
                    tr.className = 'border-b border-black/[0.02] hover:bg-black/[0.015] transition-colors';
                    
                    let cpuDisplay = '-';
                    if (p.cpu_usage !== undefined) cpuDisplay = `${p.cpu_usage.toFixed(1)}%`;
                    
                    let memDisplay = '-';
                    if (p.memory_usage_mb !== undefined) memDisplay = `${p.memory_usage_mb.toFixed(1)} MB`;

                    tr.innerHTML = `
                        <td class="py-2 px-3 font-mono font-medium text-textSubtle">${p.pid}</td>
                        <td class="py-2 px-3 font-semibold text-textMain truncate max-w-[200px]" title="${escapeHtml(p.name)}">${escapeHtml(p.name)}</td>
                        <td class="py-2 px-3 font-mono text-right font-semibold text-textMain">${cpuDisplay}</td>
                        <td class="py-2 px-3 font-mono text-right text-textSubtle">${memDisplay}</td>
                    `;
                    procTableBody.appendChild(tr);
                });
            } else {
                procTableBody.innerHTML = '<tr><td colspan="4" class="text-center py-6 text-textMuted">No processes matched filter.</td></tr>';
            }
        }

        const svcTableBody = document.getElementById('svcTableBody');
        if (svcTableBody) {
            svcTableBody.innerHTML = '';
            const filteredSvc = allServices.filter(s => 
                s.name.toLowerCase().includes(filterText) || 
                s.display_name.toLowerCase().includes(filterText)
            );

            if (filteredSvc.length > 0) {
                filteredSvc.forEach(s => {
                    const tr = document.createElement('tr');
                    tr.className = 'border-b border-black/[0.02] hover:bg-black/[0.015] transition-colors';
                    const isRunning = s.status === 'Running';
                    tr.innerHTML = `
                        <td class="py-2 px-3 font-mono text-[11.5px] font-semibold text-textSubtle truncate max-w-[120px]" title="${escapeHtml(s.name)}">${escapeHtml(s.name)}</td>
                        <td class="py-2 px-3 text-textMain truncate max-w-[150px]" title="${escapeHtml(s.display_name)}">${escapeHtml(s.display_name)}</td>
                        <td class="py-2 px-3 text-textSubtle text-[11px]">${escapeHtml(s.start_type || 'Automatic')}</td>
                        <td class="py-2 px-3 text-right">
                            <span class="px-1.5 py-0.5 rounded text-[9.5px] font-bold ${isRunning ? 'bg-success/15 text-success' : 'bg-danger/15 text-danger'}">
                                ${escapeHtml(s.status)}
                            </span>
                        </td>
                    `;
                    svcTableBody.appendChild(tr);
                });
            } else {
                svcTableBody.innerHTML = '<tr><td colspan="4" class="text-center py-6 text-textMuted">No services matched filter.</td></tr>';
            }
        }
    }

    function renderEventLogsTable() {
        const selectedLevel = document.getElementById('logLevelSelect').value;
        const selectedLogName = document.getElementById('logNameSelect').value;
        const filterText = document.getElementById('logSearchInput').value.trim().toLowerCase();

        const logsTableBody = document.getElementById('logsTableBody');
        if (!logsTableBody) return;

        logsTableBody.innerHTML = '';
        const filteredLogs = allLogs.filter(l => {
            const matchesLevel = selectedLevel === 'ALL' || l.type.toLowerCase() === selectedLevel.toLowerCase();
            const matchesLogName = selectedLogName === 'ALL' || l.log_name.toLowerCase() === selectedLogName.toLowerCase();
            const matchesSearch = !filterText || 
                l.message.toLowerCase().includes(filterText) ||
                l.source.toLowerCase().includes(filterText) ||
                l.event_id.toString().includes(filterText);
            return matchesLevel && matchesLogName && matchesSearch;
        });

        if (filteredLogs.length > 0) {
            filteredLogs.forEach(l => {
                const tr = document.createElement('tr');
                tr.className = 'border-b border-black/[0.02] hover:bg-black/[0.015] transition-colors cursor-pointer';
                const type = l.type || 'Information';
                const badgeClass = type === 'Critical' ? 'bg-danger text-white' 
                    : type === 'Error' ? 'bg-danger/15 text-danger'
                    : type === 'Warning' ? 'bg-warning/15 text-warning'
                    : 'bg-black/5 text-textSubtle';
                
                tr.innerHTML = `
                    <td class="py-2.5 px-4 font-mono text-[11px] text-textMuted">${escapeHtml(l.time_created)}</td>
                    <td class="py-2.5 px-4 font-semibold text-textSubtle">${escapeHtml(l.log_name)}</td>
                    <td class="py-2.5 px-4 text-textMain truncate max-w-[120px] font-mono text-[11px]" title="${escapeHtml(l.source)}">${escapeHtml(l.source)}</td>
                    <td class="py-2.5 px-4 font-mono text-textSubtle">${l.event_id}</td>
                    <td class="py-2.5 px-4"><span class="px-1.5 py-0.5 rounded text-[9.5px] font-bold ${badgeClass}">${type}</span></td>
                    <td class="py-2.5 px-4 text-textMain font-mono text-[11px] max-w-[300px] truncate" title="${escapeHtml(l.message)}">${escapeHtml(l.message)}</td>
                `;
                
                tr.addEventListener('click', () => {
                    const nextRow = tr.nextSibling;
                    if (nextRow && nextRow.classList.contains('log-detail-row')) {
                        nextRow.remove();
                    } else {
                        const detailRow = document.createElement('tr');
                        detailRow.className = 'log-detail-row bg-black/[0.02] border-b border-black/[0.03]';
                        detailRow.innerHTML = `
                            <td colspan="6" class="p-4 text-[12.5px] text-textMain font-mono leading-relaxed whitespace-pre-wrap select-all">
                                <strong>Source:</strong> ${escapeHtml(l.source)} | <strong>Event ID:</strong> ${l.event_id} | <strong>Log:</strong> ${escapeHtml(l.log_name)}
                                <div class="mt-2 border-t border-borderSubtle pt-2 text-textSubtle font-sans">${escapeHtml(l.message)}</div>
                            </td>
                        `;
                        tr.after(detailRow);
                    }
                });

                logsTableBody.appendChild(tr);
            });
        } else {
            logsTableBody.innerHTML = '<tr><td colspan="6" class="text-center py-8 text-textMuted">No event logs matched criteria.</td></tr>';
        }
    }

    function renderSoftwareTable() {
        const filterText = document.getElementById('softSearchInput').value.trim().toLowerCase();

        const softTableBody = document.getElementById('softTableBody');
        if (softTableBody) {
            softTableBody.innerHTML = '';
            const filteredApps = allInstalledApps.filter(app => 
                app.name.toLowerCase().includes(filterText) ||
                app.version.toLowerCase().includes(filterText)
            );

            if (filteredApps.length > 0) {
                filteredApps.forEach(app => {
                    const tr = document.createElement('tr');
                    tr.className = 'border-b border-black/[0.02] hover:bg-black/[0.015] transition-colors';
                    tr.innerHTML = `
                        <td class="py-2.5 px-4 font-semibold text-textMain truncate max-w-[300px]" title="${escapeHtml(app.name)}">${escapeHtml(app.name)}</td>
                        <td class="py-2.5 px-4 font-mono text-[11.5px] text-textSubtle">${escapeHtml(app.version)}</td>
                        <td class="py-2.5 px-4 font-mono text-textMuted text-right">${escapeHtml(app.install_date || 'Unknown')}</td>
                    `;
                    softTableBody.appendChild(tr);
                });
            } else {
                softTableBody.innerHTML = '<tr><td colspan="3" class="text-center py-6 text-textMuted">No applications matched criteria.</td></tr>';
            }
        }

        const startupTableBody = document.getElementById('startupTableBody');
        if (startupTableBody) {
            startupTableBody.innerHTML = '';
            if (allStartupApps.length > 0) {
                allStartupApps.forEach(app => {
                    const tr = document.createElement('tr');
                    tr.className = 'border-b border-black/[0.02] hover:bg-black/[0.015] transition-colors';
                    tr.innerHTML = `
                        <td class="py-2 px-3 font-semibold text-textMain truncate max-w-[150px]" title="${escapeHtml(app.name)}">${escapeHtml(app.name)}</td>
                        <td class="py-2 px-3 text-[11.5px] text-textSubtle truncate max-w-[200px]" title="${escapeHtml(app.path)}">${escapeHtml(app.location || 'Folder')}</td>
                    `;
                    startupTableBody.appendChild(tr);
                });
            } else {
                startupTableBody.innerHTML = '<tr><td colspan="2" class="text-center py-6 text-textMuted">No startup applications registered.</td></tr>';
            }
        }
    }

    function bindSearchFilters() {
        document.getElementById('procSearchInput').addEventListener('input', () => renderProcessesAndServicesTable());
        document.getElementById('logLevelSelect').addEventListener('change', () => renderEventLogsTable());
        document.getElementById('logNameSelect').addEventListener('change', () => renderEventLogsTable());
        document.getElementById('logSearchInput').addEventListener('input', () => renderEventLogsTable());
        document.getElementById('softSearchInput').addEventListener('input', () => renderSoftwareTable());
    }

    function bindTabsNavigation() {
        const tabButtons = document.querySelectorAll('.tab-btn');
        const tabContents = document.querySelectorAll('.tab-content');

        tabButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                const targetId = btn.id.replace('btnTab', 'content');
                
                // Hide all contents & reset styles
                tabContents.forEach(c => c.classList.add('hidden'));
                tabButtons.forEach(b => {
                    b.className = 'tab-btn pb-2 border-b-2 border-transparent hover:text-textMain text-textSubtle transition-all flex items-center gap-1.5 focus:outline-none';
                });

                // Set clicked active
                btn.className = 'tab-btn pb-2 border-b-2 border-primary text-primary transition-all flex items-center gap-1.5 focus:outline-none';
                
                // Show corresponding pane
                const targetContent = document.getElementById(targetId);
                if (targetContent) {
                    targetContent.classList.remove('hidden');
                }

                if (window.lucide) {
                    window.lucide.createIcons();
                }
            });
        });
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

    function formatTimestamp(ts) {
        if (!ts) return '-';
        return ts;
    }

    function formatTimeOnly(ts) {
        if (!ts) return '';
        const parts = ts.split(' ');
        if (parts.length > 1) {
            return parts[1];
        }
        return ts;
    }

    // User Assignment Rendering
    function renderUserAssignment(dev) {
        const actionsContainer = document.getElementById('assignmentActions');
        const infoContainer = document.getElementById('assignedUserInfo');
        if (!actionsContainer || !infoContainer) return;

        actionsContainer.innerHTML = '';
        infoContainer.innerHTML = '';

        if (dev.assigned_user_name) {
            infoContainer.innerHTML = `
                <div class="flex flex-col gap-0.5">
                    <span class="text-[10px] font-mono uppercase tracking-wider text-textSubtle">Full Name</span>
                    <span class="font-semibold text-textMain">${escapeHtml(dev.assigned_user_name)}</span>
                </div>
                <div class="flex flex-col gap-0.5">
                    <span class="text-[10px] font-mono uppercase tracking-wider text-textSubtle">Email Address</span>
                    <span class="font-semibold text-textMain">${escapeHtml(dev.assigned_user_email)}</span>
                </div>
                <div class="flex flex-col gap-0.5">
                    <span class="text-[10px] font-mono uppercase tracking-wider text-textSubtle">Department</span>
                    <span class="font-semibold text-textMain">${dev.assigned_user_dept ? escapeHtml(dev.assigned_user_dept) : 'N/A'}</span>
                </div>
            `;

            actionsContainer.innerHTML = `
                <button id="btnChangeUser" class="border border-borderSubtle bg-surface text-textMain hover:bg-black/[0.02] px-2.5 py-1 rounded text-[11px] font-medium transition-colors flex items-center gap-1">
                    <i data-lucide="user-cog" class="w-3.5 h-3.5 text-textSubtle"></i> Change User
                </button>
                <button id="btnUnassignUser" class="border border-danger/20 bg-danger/5 hover:bg-danger/10 text-danger px-2.5 py-1 rounded text-[11px] font-medium transition-colors flex items-center gap-1">
                    <i data-lucide="user-minus" class="w-3.5 h-3.5 text-danger"></i> Remove Assignment
                </button>
            `;

            document.getElementById('btnChangeUser').addEventListener('click', () => openAssignmentModal());
            document.getElementById('btnUnassignUser').addEventListener('click', () => handleUnassign());
        } else {
            infoContainer.innerHTML = `
                <div class="col-span-3 text-textSubtle text-[12.5px] italic flex items-center gap-2">
                    <i data-lucide="info" class="w-4 h-4 text-textSubtle"></i> No user is currently assigned to this device.
                </div>
            `;

            actionsContainer.innerHTML = `
                <button id="btnAssignUser" class="bg-primary hover:bg-primaryHover text-white px-2.5 py-1 rounded text-[11px] font-semibold transition-colors flex items-center gap-1 shadow-sm">
                    <i data-lucide="user-plus" class="w-3.5 h-3.5 flex items-center"></i> Assign User
                </button>
            `;

            document.getElementById('btnAssignUser').addEventListener('click', () => openAssignmentModal());
        }

        if (window.lucide) {
            window.lucide.createIcons();
        }
    }

    const modalEl = document.getElementById('assignmentModal');
    const closeBtn = document.getElementById('closeModalBtn');
    const tabSelectExisting = document.getElementById('tabSelectExisting');
    const tabCreateNew = document.getElementById('tabCreateNew');
    const formSelectExisting = document.getElementById('formSelectExisting');
    const formCreateNew = document.getElementById('formCreateNew');
    
    if (closeBtn) {
        closeBtn.addEventListener('click', () => closeAssignmentModal());
    }

    if (tabSelectExisting && tabCreateNew) {
        tabSelectExisting.addEventListener('click', () => {
            tabSelectExisting.className = 'pb-2 border-b-2 border-primary text-primary transition-all';
            tabCreateNew.className = 'pb-2 border-b-2 border-transparent text-textSubtle hover:text-textMain transition-all';
            formSelectExisting.classList.remove('hidden');
            formCreateNew.classList.add('hidden');
        });

        tabCreateNew.addEventListener('click', () => {
            tabCreateNew.className = 'pb-2 border-b-2 border-primary text-primary transition-all';
            tabSelectExisting.className = 'pb-2 border-b-2 border-transparent text-textSubtle hover:text-textMain transition-all';
            formCreateNew.classList.remove('hidden');
            formSelectExisting.classList.add('hidden');
        });
    }

    async function openAssignmentModal() {
        modalEl.classList.remove('hidden');
        
        document.getElementById('newFullName').value = '';
        document.getElementById('newEmail').value = '';
        document.getElementById('newDepartment').value = '';
        
        tabSelectExisting.click();

        try {
            const users = await api.getDeviceUsers();
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
        } catch (e) {
            console.error('Failed to load device users:', e);
        }
    }

    function closeAssignmentModal() {
        modalEl.classList.add('hidden');
    }

    document.getElementById('submitAssignExisting').addEventListener('click', async () => {
        const userId = document.getElementById('userDropdown').value;
        if (!userId) {
            alert('Please select a user to assign.');
            return;
        }

        try {
            const updated = await api.assignDeviceUser(deviceId, parseInt(userId));
            if (updated) {
                renderUserAssignment(updated);
                closeAssignmentModal();
            }
        } catch (e) {
            alert('Failed to assign user: ' + e.message);
        }
    });

    document.getElementById('submitCreateAndAssign').addEventListener('click', async () => {
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
                const updated = await api.assignDeviceUser(deviceId, newUser.id);
                if (updated) {
                    renderUserAssignment(updated);
                    closeAssignmentModal();
                }
            }
        } catch (e) {
            alert('Failed to create and assign user: ' + e.message);
        }
    });

    async function handleUnassign() {
        if (!confirm('Are you sure you want to remove the assigned user from this device?')) {
            return;
        }
        try {
            const updated = await api.assignDeviceUser(deviceId, null);
            renderUserAssignment(updated);
        } catch (e) {
            alert('Failed to unassign user: ' + e.message);
        }
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

    // Bind Remove Device Button
    const removeBtn = document.getElementById('removeDeviceBtn');
    if (removeBtn) {
        removeBtn.addEventListener('click', async () => {
            if (confirm('CRITICAL ACTION: Are you sure you want to permanently remove this device and delete all its telemetry history? This action cannot be undone.')) {
                try {
                    await api.deleteDevice(deviceId);
                    alert('Device removed successfully.');
                    window.location.href = 'devices.html';
                } catch (e) {
                    alert('Failed to remove device: ' + e.message);
                }
            }
        });
    }

    // Bind Restart Agent Button
    const restartBtn = document.getElementById('restartAgentBtn');
    if (restartBtn) {
        restartBtn.addEventListener('click', async () => {
            if (confirm('Are you sure you want to trigger a remote restart of the monitoring agent service on this node?')) {
                try {
                    await api.restartDeviceAgent(deviceId);
                    alert('Restart command dispatched successfully.');
                    updateStatusBadge('restarting');
                } catch (e) {
                    alert('Failed to dispatch restart command: ' + e.message);
                }
            }
        });
    }
});
