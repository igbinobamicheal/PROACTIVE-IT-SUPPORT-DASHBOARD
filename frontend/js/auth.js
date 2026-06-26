// Authentication page logic
document.addEventListener('DOMContentLoaded', () => {
    const loginForm = document.getElementById('loginForm');
    const loginBtn = document.getElementById('loginBtn');
    const btnText = loginBtn.querySelector('.btn-text');
    const btnSpinner = loginBtn.querySelector('.btn-spinner');
    const errorMessage = document.getElementById('errorMessage');

    if (!loginForm) return;

    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const username = document.getElementById('username').value.trim();
        const password = document.getElementById('password').value;

        // Reset UI
        errorMessage.classList.add('hidden');
        errorMessage.textContent = '';
        btnText.classList.add('hidden');
        btnSpinner.classList.remove('hidden');
        loginBtn.disabled = true;

        try {
            const data = await api.login(username, password);
            if (data && data.token) {
                localStorage.setItem('jwt_token', data.token);
                localStorage.setItem('username', username);
                window.location.href = 'dashboard.html';
            } else {
                throw new Error('Authentication failed');
            }
        } catch (err) {
            errorMessage.textContent = err.message || 'Network error occurred';
            errorMessage.classList.remove('hidden');
            
            // Restore UI
            btnText.classList.remove('hidden');
            btnSpinner.classList.add('hidden');
            loginBtn.disabled = false;
        }
    });
});
