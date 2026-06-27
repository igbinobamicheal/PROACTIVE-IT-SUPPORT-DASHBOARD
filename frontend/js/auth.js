// Authentication page logic (Login & Signup)
document.addEventListener('DOMContentLoaded', () => {
    // 1. Check if login page query parameters have signup success status
    const urlParams = new URLSearchParams(window.location.search);
    const signupSuccess = urlParams.get('signup_success');
    const errorMessage = document.getElementById('errorMessage');
    
    if (signupSuccess && errorMessage) {
        errorMessage.classList.remove('hidden');
        errorMessage.classList.remove('bg-danger/10', 'text-danger', 'border-danger/20');
        // Add success class styling
        errorMessage.classList.add('bg-success/10', 'text-success', 'border-success/20');
        errorMessage.textContent = 'Registration successful! Please sign in with your credentials.';
    }

    // 2. Login Form Handling
    const loginForm = document.getElementById('loginForm');
    if (loginForm) {
        const loginBtn = document.getElementById('loginBtn');
        const btnText = loginBtn ? loginBtn.querySelector('.btn-text') : null;
        const btnSpinner = loginBtn ? loginBtn.querySelector('.btn-spinner') : null;

        loginForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const username = document.getElementById('username').value.trim();
            const password = document.getElementById('password').value;

            // Reset UI
            if (errorMessage) {
                errorMessage.classList.add('hidden');
                errorMessage.classList.remove('bg-success/10', 'text-success', 'border-success/20');
                errorMessage.classList.add('bg-danger/10', 'text-danger', 'border-danger/20');
                errorMessage.textContent = '';
            }
            if (btnText) btnText.classList.add('hidden');
            if (btnSpinner) btnSpinner.classList.remove('hidden');
            if (loginBtn) loginBtn.disabled = true;

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
                if (errorMessage) {
                    errorMessage.textContent = err.message || 'Network error occurred';
                    errorMessage.classList.remove('hidden');
                }
                
                // Restore UI
                if (btnText) btnText.classList.remove('hidden');
                if (btnSpinner) btnSpinner.classList.add('hidden');
                if (loginBtn) loginBtn.disabled = false;
            }
        });
    }

    // 3. Signup Form Handling
    const signupForm = document.getElementById('signupForm');
    if (signupForm) {
        const signupBtn = document.getElementById('signupBtn');
        const btnText = signupBtn ? signupBtn.querySelector('.btn-text') : null;
        const btnSpinner = signupBtn ? signupBtn.querySelector('.btn-spinner') : null;

        signupForm.addEventListener('submit', async (e) => {
            e.preventDefault();

            const username = document.getElementById('username').value.trim();
            const password = document.getElementById('password').value;
            const confirmPassword = document.getElementById('confirmPassword').value;

            if (errorMessage) {
                errorMessage.classList.add('hidden');
                errorMessage.textContent = '';
            }

            if (password !== confirmPassword) {
                if (errorMessage) {
                    errorMessage.classList.remove('bg-success/10', 'text-success', 'border-success/20');
                    errorMessage.classList.add('bg-danger/10', 'text-danger', 'border-danger/20');
                    errorMessage.textContent = 'Passwords do not match.';
                    errorMessage.classList.remove('hidden');
                }
                return;
            }

            // Update UI
            if (btnText) btnText.classList.add('hidden');
            if (btnSpinner) btnSpinner.classList.remove('hidden');
            if (signupBtn) signupBtn.disabled = true;

            try {
                const response = await api.signup(username, password);
                // Redirect to login on success
                window.location.href = 'login.html?signup_success=true';
            } catch (err) {
                if (errorMessage) {
                    errorMessage.classList.remove('bg-success/10', 'text-success', 'border-success/20');
                    errorMessage.classList.add('bg-danger/10', 'text-danger', 'border-danger/20');
                    errorMessage.textContent = err.message || 'Failed to create account';
                    errorMessage.classList.remove('hidden');
                }

                // Restore UI
                if (btnText) btnText.classList.remove('hidden');
                if (btnSpinner) btnSpinner.classList.add('hidden');
                if (signupBtn) signupBtn.disabled = false;
            }
        });
    }
});
