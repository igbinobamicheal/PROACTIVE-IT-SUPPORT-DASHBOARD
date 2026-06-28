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

        try {
            const response = await fetch(`${API_BASE}${endpoint}`, config);
            
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

    createRegistrationToken() {
        return this.request('/registration-tokens', 'POST');
    },

    revokeRegistrationToken(token) {
        return this.request('/registration-tokens/revoke', 'POST', { token });
    }
};
