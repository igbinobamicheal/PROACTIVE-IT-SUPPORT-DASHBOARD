(function() {
    document.addEventListener('DOMContentLoaded', () => {
        const sidebar = document.querySelector('nav');
        if (!sidebar) return; // Not a dashboard page with sidebar

        sidebar.id = 'sidebar';

        // 1. Inject Styles
        const style = document.createElement('style');
        style.textContent = `
            #sidebar {
                transition: width 0.22s cubic-bezier(0.4, 0, 0.2, 1), transform 0.22s cubic-bezier(0.4, 0, 0.2, 1), padding 0.22s cubic-bezier(0.4, 0, 0.2, 1), border-color 0.22s ease;
            }
            
            /* Collapsed State Desktop */
            body.sidebar-collapsed #sidebar {
                width: 0px !important;
                padding-left: 0px !important;
                padding-right: 0px !important;
                border-right-width: 0px !important;
                overflow: hidden;
            }
            
            /* Hide expand button when sidebar is open on desktop */
            #sidebarExpand {
                display: none;
            }
            body.sidebar-collapsed #sidebarExpand {
                display: inline-flex;
            }
            
            /* Mobile Styles & Responsiveness */
            @media (max-width: 767px) {
                #sidebar {
                    position: fixed !important;
                    height: 100vh !important;
                    transform: translateX(-100%) !important;
                    width: 240px !important;
                    z-index: 50 !important;
                    box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.7);
                    background-color: #0E0E10 !important;
                }
                #sidebar.mobile-open {
                    transform: translateX(0) !important;
                }
                #sidebarExpand {
                    display: inline-flex !important;
                }
                #sidebarCollapse {
                    display: none !important;
                }
            }
        `;
        document.head.appendChild(style);

        // 2. Inject Collapse Button inside Sidebar Header
        const logoContainer = sidebar.querySelector('.px-5'); // selector for logo wrapper
        if (logoContainer) {
            logoContainer.classList.add('justify-between');
            
            const collapseBtn = document.createElement('button');
            collapseBtn.id = 'sidebarCollapse';
            collapseBtn.className = 'p-1 rounded text-textSubtle hover:text-textMain hover:bg-white/[0.04] transition-colors md:block hidden';
            collapseBtn.innerHTML = '<i data-lucide="chevron-left" class="w-4 h-4"></i>';
            logoContainer.appendChild(collapseBtn);
        }

        // 3. Inject Expand Button in Main Header
        const headerPath = document.querySelector('header > div.flex.items-center');
        if (headerPath) {
            const expandBtn = document.createElement('button');
            expandBtn.id = 'sidebarExpand';
            expandBtn.className = 'p-1.5 rounded border border-borderSubtle bg-surface text-textSubtle hover:text-textMain transition-colors mr-2';
            expandBtn.innerHTML = '<i data-lucide="menu" class="w-4 h-4"></i>';
            headerPath.insertBefore(expandBtn, headerPath.firstChild);
        }

        // 4. Inject Mobile Backdrop Overlay
        const overlay = document.createElement('div');
        overlay.id = 'sidebarOverlay';
        overlay.className = 'fixed inset-0 bg-black/50 backdrop-blur-sm z-40 hidden transition-opacity duration-300 opacity-0';
        document.body.appendChild(overlay);

        // 5. Run local SVG icons parsing to display newly injected icons
        if (window.lucide) {
            window.lucide.createIcons();
        }

        // 6. Restore Collapse State on Load
        const isCollapsed = localStorage.getItem('sidebar_collapsed') === 'true';
        if (isCollapsed && window.innerWidth >= 768) {
            document.body.classList.add('sidebar-collapsed');
        }

        // 7. Click Handlers
        const collapseBtn = document.getElementById('sidebarCollapse');
        if (collapseBtn) {
            collapseBtn.addEventListener('click', () => {
                document.body.classList.add('sidebar-collapsed');
                localStorage.setItem('sidebar_collapsed', 'true');
            });
        }

        const expandBtn = document.getElementById('sidebarExpand');
        if (expandBtn) {
            expandBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                if (window.innerWidth < 768) {
                    // Mobile Drawer toggle
                    sidebar.classList.toggle('mobile-open');
                    if (sidebar.classList.contains('mobile-open')) {
                        overlay.classList.remove('hidden');
                        setTimeout(() => {
                            overlay.classList.add('opacity-100');
                        }, 50);
                    } else {
                        closeMobileSidebar();
                    }
                } else {
                    // Desktop expand
                    document.body.classList.remove('sidebar-collapsed');
                    localStorage.setItem('sidebar_collapsed', 'false');
                }
            });
        }

        overlay.addEventListener('click', closeMobileSidebar);

        function closeMobileSidebar() {
            sidebar.classList.remove('mobile-open');
            overlay.classList.remove('opacity-100');
            setTimeout(() => {
                overlay.classList.add('hidden');
            }, 250);
        }
    });
})();
