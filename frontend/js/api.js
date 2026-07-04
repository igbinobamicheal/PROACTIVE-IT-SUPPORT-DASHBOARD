// API client for communication with the Crow backend
// Configure API_BASE_URL before loading this script to override for production.
// Example: <script>window.API_BASE_URL = 'https://your-backend.example.com/api';</script>
const API_BASE = window.API_BASE_URL || 'http://localhost:8080/api';

const api = {
    /**
     * Sends an HTTP request to the backend
     */
    async request(endpoint, method = 'GET', body = null, requireAuth = true) {
        const headers = {
            'Content-Type': 'application/json'
        };

        if (requireAuth) {
            const token = localStorage.getItem('jwt_token');
            if (!token) {
                window.location.href = 'login.html';
                return null;
            }
            headers['Authorization'] = `Bearer ${token}`;
        }

        const config = {
            method,
            headers
        };

        if (body) {
            config.body = JSON.stringify(body);
        }

        // Add timeout to prevent UI hanging if backend is unresponsive
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 15000);
        config.signal = controller.signal;

        try {
            const response = await fetch(`${API_BASE}${endpoint}`, config);
            clearTimeout(timeoutId);
            
            if (response.status === 401 && requireAuth) {
                // Token expired or invalid
                localStorage.removeItem('jwt_token');
                localStorage.removeItem('username');
                window.location.href = 'login.html';
                return null;
            }

            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error || 'Server error occurred');
            }
            return data;
        } catch (error) {
            clearTimeout(timeoutId);
            if (error.name === 'AbortError') {
                console.error(`API Timeout on ${method} ${endpoint}: Request timed out after 15 seconds`);
                throw new Error('Request timed out. Please check your network connection.');
            }
            console.error(`API Error on ${method} ${endpoint}:`, error);
            throw error;
        }
    },

    /**
     * Returns the base URL (without /api) for non-JSON endpoints (SSE, file downloads)
     */
    getBaseOrigin() {
        // Strip trailing '/api' to get the server origin
        return API_BASE.replace(/\/api\/?$/, '');
    },

    login(username, password) {
        return this.request('/login', 'POST', { username, password }, false);
    },
    
    signup(username, password) {
        return this.request('/signup', 'POST', { username, password }, false);
    },

    registerDevice(name, ipAddress) {
        return this.request('/register-device', 'POST', { name, ip_address: ipAddress });
    },

    getDevices() {
        return this.request('/devices', 'GET');
    },

    getDevice(id) {
        return this.request(`/device/${id}`, 'GET');
    },

    getDeviceMetrics(id) {
        return this.request(`/device/${id}/metrics`, 'GET');
    },

    getAlerts(activeOnly = false) {
        return this.request(`/alerts${activeOnly ? '?active=true' : ''}`, 'GET');
    },

    getRegistrationTokens() {
        return this.request('/registration-tokens', 'GET');
    },

    createRegistrationToken(assignedUserId = null) {
        return this.request('/registration-tokens', 'POST', assignedUserId ? { assigned_user_id: assignedUserId } : {});
    },

    revokeRegistrationToken(token) {
        return this.request('/registration-tokens/revoke', 'POST', { token });
    },

    getDeviceUsers() {
        return this.request('/device-users', 'GET');
    },

    createDeviceUser(fullName, email, department = '') {
        return this.request('/device-users', 'POST', { full_name: fullName, email, department });
    },

    ensureDeviceUser(fullName, email, department = '') {
        return this.request('/device-users/ensure', 'POST', { full_name: fullName, email, department });
    },

    assignDeviceUser(deviceId, userId) {
        return this.request(`/device/${deviceId}/assign`, 'POST', { user_id: userId });
    },

    updateDeviceUser(userId, data) {
        return this.request(`/device-users/${userId}`, 'PUT', data);
    },

    renameDepartment(oldName, newName) {
        return this.request('/device-users/department', 'PUT', { old_name: oldName, new_name: newName });
    },

    getMetricTrends() {
        return this.request('/metrics/trends', 'GET');
    },

    getDeviceDiagnostics(id) {
        return this.request(`/device/${id}/diagnostics`, 'GET');
    },

    deleteDevice(id) {
        return this.request(`/device/${id}`, 'DELETE');
    },

    restartDeviceAgent(id) {
        return this.request(`/device/${id}/restart`, 'POST');
    }
};

window.api = api;
