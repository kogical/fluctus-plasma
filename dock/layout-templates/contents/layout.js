var panel = new Panel;
panel.location = "bottom";
// Exact headroom for default 48px icons * 1.8x zoom (87px) + 0.55x bounce (48px) + 12px margins
panel.height = 147;
panel.lengthMode = "fit";
panel.alignment = "center";
panel.hiding = "dodgewindows";
panel.floating = false;

panel.addWidget("com.kogical.fluctus.dock");
