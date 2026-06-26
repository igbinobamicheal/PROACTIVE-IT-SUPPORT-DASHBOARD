// Helper utilities for the dashboard frontend

const utils = {
    /**
     * Formats bytes to human-readable size
     */
    formatBytes(bytes, decimals = 2) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const dm = decimals < 0 ? 0 : decimals;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
    },

    /**
     * Formats uptime seconds into human-readable duration
     */
    formatUptime(seconds) {
        const d = Math.floor(seconds / (3600*24));
        const h = Math.floor(seconds % (3600*24) / 3600);
        const m = Math.floor(seconds % 3600 / 60);
        const s = Math.floor(seconds % 60);

        const dDisplay = d > 0 ? d + (d === 1 ? " day, " : " days, ") : "";
        const hDisplay = h > 0 ? h + (h === 1 ? " hr, " : " hrs, ") : "";
        const mDisplay = m > 0 ? m + (m === 1 ? " min, " : " mins, ") : "";
        const sDisplay = s > 0 ? s + (s === 1 ? " sec" : " secs") : "0 secs";
        return dDisplay + hDisplay + mDisplay + sDisplay;
    },

    /**
     * Formats timestamp to localized datetime
     */
    formatDateTime(dateStr) {
        if (!dateStr) return 'N/A';
        const d = new Date(dateStr);
        return d.toLocaleString();
    }
};
