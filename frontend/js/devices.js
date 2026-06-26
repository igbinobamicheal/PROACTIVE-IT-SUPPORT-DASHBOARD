// Devices table loader
document.addEventListener('DOMContentLoaded', async () => {
    const tableBody = document.getElementById('devicesTableBody');
    if (!tableBody) return;

    try {
        const devices = await api.getDevices();
        tableBody.innerHTML = '';
        
        if (devices.length === 0) {
            tableBody.innerHTML = `<tr><td colspan="5" style="padding: 24px; text-align: center; color: var(--text-secondary);">No devices registered yet.</td></tr>`;
            return;
        }

        devices.forEach(d => {
            const tr = document.createElement('tr');
            tr.style.borderBottom = '1px solid var(--border-glass)';
            tr.innerHTML = `
                <td style="padding: 12px;">${d.id}</td>
                <td style="padding: 12px; font-weight: 600;">${d.name}</td>
                <td style="padding: 12px; color: var(--text-secondary);">${d.ip_address}</td>
                <td style="padding: 12px;"><span style="color: ${d.status === 'online' ? 'var(--success)' : 'var(--danger)'};">${d.status.toUpperCase()}</span></td>
                <td style="padding: 12px;"><a href="device-details.html?id=${d.id}" class="btn btn-secondary" style="padding: 6px 12px; font-size: 13px;">View Metrics</a></td>
            `;
            tableBody.appendChild(tr);
        });
    } catch (err) {
        tableBody.innerHTML = `<tr><td colspan="5" style="padding: 24px; text-align: center; color: var(--danger);">Failed to load devices: ${err.message}</td></tr>`;
    }
});
