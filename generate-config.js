const fs = require('fs');
const path = require('path');

// Read API_URL from environment, fallback to localhost for development
const apiUrl = process.env.API_URL || 'http://localhost:8080/api';

// Create the config content
const content = `// Auto-generated during build. Do not modify manually.
window.API_BASE_URL = "${apiUrl}";
`;

// Write to frontend/js/config.js
const targetDir = path.join(__dirname, 'frontend', 'js');
if (!fs.existsSync(targetDir)) {
    fs.mkdirSync(targetDir, { recursive: true });
}
fs.writeFileSync(path.join(targetDir, 'config.js'), content);
console.log(`[Vercel Build] Configured API_BASE_URL to: ${apiUrl}`);
