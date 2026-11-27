image_fe_logo.png = main-menu logo of Warzone2100 Mobile (original plates + MOBILE plate).
It replaces the file of the same name inside the game data (base.wz). Then set kLogoImageIsMobile = true
in displayLogo() in src/frontend.cpp so the extra "MOBILE" text is not drawn twice.
