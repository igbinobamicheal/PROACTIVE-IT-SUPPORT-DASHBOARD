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

        // 2. Fetch history and initialize charts/table
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

        // 4. Initialize real-time updates via SSE
        initializeSSE();

    } catch (err) {
        console.error('Failed to load device details:', err);
    }

    function updateStatusBadge(status) {
        const badge = document.getElementById('statusBadge');
        const text = document.getElementById('deviceStatusSub');
        if (!badge || !text) return;

        badge.textContent = status.toUpperCase();
        if (status === 'online') {
            badge.className = 'inline-flex items-center px-4 py-1.5 rounded-full bg-green-50 text-green-700 border border-green-200 text-[12px] font-black uppercase tracking-wider shadow-sm';
            text.innerHTML = '<span class="inline-block w-2 h-2 rounded-full bg-green-500 animate-pulse mr-2"></span><span class="text-[15px] font-medium text-[#0c0f0a]/60">Active monitoring agent online</span>';
        } else {
            badge.className = 'inline-flex items-center px-4 py-1.5 rounded-full bg-red-50 text-red-700 border border-red-200 text-[12px] font-black uppercase tracking-wider shadow-sm';
            text.innerHTML = '<span class="material-symbols-outlined text-red-500 text-[16px] mr-1">cancel</span><span class="text-[15px] font-medium text-[#0c0f0a]/60">Agent offline (no contact)</span>';
        }
    }

    function updateStatsCards(m) {
        document.getElementById('cardCpu').textContent = `${m.cpu_usage.toFixed(1)}%`;
        document.getElementById('cardRam').textContent = `${m.ram_usage.toFixed(1)}%`;
        document.getElementById('cardDisk').textContent = `${m.disk_usage.toFixed(1)}%`;
        document.getElementById('cardNetwork').innerHTML = `
            <div class="flex items-center gap-1.5 mb-1">
                <span class="material-symbols-outlined text-[16px] text-blue-500">arrow_downward</span>
                <span class="text-[14px] font-medium text-[#0c0f0a]/60">In:</span>
                <span class="text-[14px] font-bold text-[#0c0f0a]">${m.network_in.toFixed(1)} MB</span>
            </div>
            <div class="flex items-center gap-1.5">
                <span class="material-symbols-outlined text-[16px] text-amber-500">arrow_upward</span>
                <span class="text-[14px] font-medium text-[#0c0f0a]/60">Out:</span>
                <span class="text-[14px] font-bold text-[#0c0f0a]">${m.network_out.toFixed(1)} MB</span>
            </div>
        `;

        // Highlight alert thresholds (e.g. > 85%)
        highlightThreshold('cardCpu', m.cpu_usage);
        highlightThreshold('cardRam', m.ram_usage);
        highlightThreshold('cardDisk', m.disk_usage);
    }

    function highlightThreshold(elementId, value) {
        const el = document.getElementById(elementId);
        if (value > 85) {
            el.style.color = '#ba1a1a';
            el.parentElement.parentElement.style.borderColor = 'rgba(186, 26, 26, 0.4)';
        } else if (value > 70) {
            el.style.color = '#86750d';
            el.parentElement.parentElement.style.borderColor = 'rgba(255, 219, 15, 0.4)';
        } else {
            el.style.color = '#0c0f0a';
            el.parentElement.parentElement.style.borderColor = 'rgba(12, 15, 10, 0.05)';
        }
    }

    function renderHistoryTable(history) {
        const tbody = document.getElementById('metricsHistoryBody');
        if (!tbody) return;
        tbody.innerHTML = '';

        if (history.length === 0) {
            tbody.innerHTML = `<tr><td colspan="7" class="text-center text-[#0c0f0a]/40 py-6 text-[14px]">No metrics history logged yet.</td></tr>`;
            return;
        }

        // Display newest reports first
        const sortedHistory = [...history].reverse();
        sortedHistory.forEach(m => {
            const tr = document.createElement('tr');
            tr.className = 'border-b border-[#f9f9f9] hover:bg-[#f9f9f9]/50 transition-colors';
            tr.innerHTML = `
                <td class="py-3.5 px-6 font-semibold text-[#0c0f0a] text-[13px]">${formatTimestamp(m.timestamp)}</td>
                <td class="py-3.5 px-6"><span style="color: ${m.cpu_usage > 85 ? '#ba1a1a' : '#0c0f0a'}" class="font-medium">${m.cpu_usage.toFixed(1)}%</span></td>
                <td class="py-3.5 px-6"><span style="color: ${m.ram_usage > 85 ? '#ba1a1a' : '#0c0f0a'}" class="font-medium">${m.ram_usage.toFixed(1)}%</span></td>
                <td class="py-3.5 px-6 font-medium text-[#0c0f0a]">${m.disk_usage.toFixed(1)}%</td>
                <td class="py-3.5 px-6 text-[#0c0f0a]/50">${m.network_in.toFixed(2)} MB</td>
                <td class="py-3.5 px-6 text-[#0c0f0a]/50">${m.network_out.toFixed(2)} MB</td>
                <td class="py-3.5 px-6 text-[#0c0f0a]/50 text-[13px]">${formatUptime(m.uptime)}</td>
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
                    backgroundColor: '#0c0f0a',
                    titleColor: '#ffffff',
                    bodyColor: '#ffffff',
                    borderColor: 'rgba(12, 15, 10, 0.2)',
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
                    grid: { color: 'rgba(12, 15, 10, 0.04)' },
                    ticks: { color: '#0c0f0a', font: { family: 'Inter', size: 10, weight: 'bold' } }
                },
                x: {
                    grid: { display: false },
                    ticks: { color: 'rgba(12, 15, 10, 0.4)', font: { family: 'Inter', size: 10 } }
                }
            }
        });

        // Initialize CPU Line Chart
        const cpuCtx = document.getElementById('cpuChart').getContext('2d');
        const cpuGrad = cpuCtx.createLinearGradient(0, 0, 0, 200);
        cpuGrad.addColorStop(0, 'rgba(255, 219, 15, 0.25)');
        cpuGrad.addColorStop(1, 'rgba(255, 219, 15, 0.0)');

        cpuChart = new Chart(cpuCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    data: cpuData,
                    borderColor: '#ffdb0f',
                    backgroundColor: cpuGrad,
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.3,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: chartOptions('CPU', 'rgba(255, 219, 15, 0.5)')
        });

        // Initialize RAM Line Chart
        const ramCtx = document.getElementById('ramChart').getContext('2d');
        const ramGrad = ramCtx.createLinearGradient(0, 0, 0, 200);
        ramGrad.addColorStop(0, 'rgba(134, 117, 13, 0.25)');
        ramGrad.addColorStop(1, 'rgba(134, 117, 13, 0.0)');

        ramChart = new Chart(ramCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    data: ramData,
                    borderColor: '#86750d',
                    backgroundColor: ramGrad,
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.3,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: chartOptions('RAM', 'rgba(134, 117, 13, 0.4)')
        });

        // Initialize Disk Line Chart
        const diskCtx = document.getElementById('diskChart').getContext('2d');
        const diskGrad = diskCtx.createLinearGradient(0, 0, 0, 200);
        diskGrad.addColorStop(0, 'rgba(12, 15, 10, 0.15)');
        diskGrad.addColorStop(1, 'rgba(12, 15, 10, 0.0)');

        diskChart = new Chart(diskCtx, {
            type: 'line',
            data: {
                labels,
                datasets: [{
                    data: diskData,
                    borderColor: '#0c0f0a',
                    backgroundColor: diskGrad,
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.3,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: chartOptions('Disk', 'rgba(12, 15, 10, 0.2)')
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

        // Listen for new metrics
        eventSource.addEventListener('metric', (e) => {
            const data = JSON.parse(e.data);
            if (data.device_id !== deviceId) return;

            console.log('[SSE] Received metric for this device:', data);

            // Update status & metadata
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
                // If it is the placeholder row, remove it first
                if (tbody.rows.length === 1 && tbody.rows[0].cells.length === 1) {
                    tbody.innerHTML = '';
                }

                const tr = document.createElement('tr');
                tr.className = 'border-b border-[#f9f9f9] hover:bg-[#f9f9f9]/50 transition-colors';
                tr.innerHTML = `
                    <td class="py-3.5 px-6 font-semibold text-[#0c0f0a] text-[13px]">${formatTimestamp(data.timestamp)}</td>
                    <td class="py-3.5 px-6"><span style="color: ${data.cpu_usage > 85 ? '#ba1a1a' : '#0c0f0a'}" class="font-medium">${data.cpu_usage.toFixed(1)}%</span></td>
                    <td class="py-3.5 px-6"><span style="color: ${data.ram_usage > 85 ? '#ba1a1a' : '#0c0f0a'}" class="font-medium">${data.ram_usage.toFixed(1)}%</span></td>
                    <td class="py-3.5 px-6 font-medium text-[#0c0f0a]">${data.disk_usage.toFixed(1)}%</td>
                    <td class="py-3.5 px-6 text-[#0c0f0a]/50">${data.network_in.toFixed(2)} MB</td>
                    <td class="py-3.5 px-6 text-[#0c0f0a]/50">${data.network_out.toFixed(2)} MB</td>
                    <td class="py-3.5 px-6 text-[#0c0f0a]/50 text-[13px]">${formatUptime(data.uptime)}</td>
                `;
                
                // Prepend to top
                tbody.insertBefore(tr, tbody.firstChild);

                // Flash transition on the new log row
                tr.style.backgroundColor = 'rgba(255, 219, 15, 0.15)';
                setTimeout(() => {
                    tr.style.transition = 'background-color 1s ease';
                    tr.style.backgroundColor = 'transparent';
                }, 100);

                // Prevent DOM bloat (limit table to last 100 entries)
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

        // Maintain sliding window of 20 items
        if (chart.data.datasets[0].data.length > 20) {
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
        }

        chart.update('none'); // Update without full recalculation transition for smooth sliding
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
        // Database timestamp returns YYYY-MM-DD HH:MM:SS
        return ts;
    }

    function formatTimeOnly(ts) {
        if (!ts) return '';
        // Extract HH:MM:SS from YYYY-MM-DD HH:MM:SS
        const parts = ts.split(' ');
        if (parts.length > 1) {
            return parts[1];
        }
        return ts;
    }
});
