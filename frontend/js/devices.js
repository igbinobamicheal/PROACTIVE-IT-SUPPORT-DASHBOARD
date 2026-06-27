// Devices table loader
document.addEventListener('DOMContentLoaded', async () => {
    const tableBody = document.getElementById('devicesTableBody');
    if (!tableBody) return;

    try {
        const devices = await api.getDevices();
        tableBody.innerHTML = '';
        
        if (devices.length === 0) {
            tableBody.innerHTML = `<tr><td colspan="5" class="py-6 text-center text-textMuted font-medium">No devices registered yet.</td></tr>`;
            return;
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

            tr.innerHTML = `
                <td class="py-3 px-4" onclick="event.stopPropagation()">
                    <input type="checkbox" class="rounded bg-white border-borderSubtle text-primary accent-primary cursor-pointer">
                </td>
                <td class="py-3 px-4">
                    <div class="font-mono font-semibold text-textMain">${escapeHtml(d.name)}</div>
                    <div class="text-[10px] text-textSubtle mt-0.5">ID: ${d.id}</div>
                </td>
                <td class="py-3 px-4">
                    <span class="flex items-center gap-2 text-textMuted">
                        <div class="w-1.5 h-1.5 rounded-full ${statusDotColor}"></div>
                        ${statusText}
                    </span>
                </td>
                <td class="py-3 px-4 font-mono text-textMuted">
                    ${escapeHtml(d.ip_address)}<br/>
                    <span class="text-[10px] text-textSubtle uppercase">agent node</span>
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
        tableBody.innerHTML = `<tr><td colspan="5" class="py-6 text-center text-danger font-semibold">Failed to load devices: ${escapeHtml(err.message)}</td></tr>`;
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
